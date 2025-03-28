//
// Created by Jiakun Yan on 12/31/19.
//
/*
 * Acknowledgement:
 *   Most content in this file is based on the source code of
 *   the Berkeley Container Library (https://github.com/berkeley-container-library/bcl).
*/

#ifndef ARL_BACKEND_BASE_HPP
#define ARL_BACKEND_BASE_HPP

#define CHECK_GEX(x)                                         \
  {                                                          \
    int err = (x);                                           \
    if (err != GASNET_OK) {                                  \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      exit(err);                                             \
    }                                                        \
  }                                                          \
  while (0)                                                  \
    ;

namespace arl::backend::internal {
extern rank_t my_proc;
extern rank_t nprocs;
extern bool finalized;

extern gex_Client_t client;
extern gex_EP_t ep;
extern gex_TM_t tm;
const char* const clientName = "ARL";

extern gasnet_seginfo_t *gasnet_seginfo;

const int GEX_NARGS = (sizeof(tag_t) + sizeof(gex_AM_Arg_t) - 1) / sizeof(gex_AM_Arg_t);
const gex_AM_Index_t gex_handler_idx = GEX_AM_INDEX_BASE;
extern std::queue<cq_entry_t> *cq_p;
extern std::mutex cq_mutex;

extern void gex_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes, gex_AM_Arg_t tag);

inline rank_t rank_me() {
  return my_proc;
}

inline rank_t rank_n() {
  return nprocs;
}

inline void barrier(bool (*do_something)() = nullptr) {
  gex_Event_t event = gex_Coll_BarrierNB(tm, 0);
  while (gex_Event_Test(event)) {
    progress();
    if (do_something != nullptr)
      do_something();
  }
}

inline void init(size_t, size_t) {
  ARL_LOG(INFO, "Initializing GASNet-EX backend\n");
  gex_Client_Init(&client, &ep, &tm, clientName, NULL, NULL, 0);

#ifndef GASNET_PAR
  throw std::runtime_error("Need to use a par build of GASNet-EX");
#endif

  my_proc = gex_System_QueryJobRank();
  nprocs = gex_System_QueryJobSize();

  gasnet_seginfo = (gasnet_seginfo_t *) malloc(sizeof(gasnet_seginfo_t) * nprocs);
  gasnet_getSegmentInfo(gasnet_seginfo, rank_n());

  //      init_malloc();
  //      init_atomics();

  finalized = false;

  gex_AM_Entry_t htable[1] = {
          {gex_handler_idx,
           (gex_AM_Fn_t) gex_reqhandler,
           GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
           GEX_NARGS}};

  CHECK_GEX(gex_EP_RegisterHandlers(ep, htable, sizeof(htable) / sizeof(gex_AM_Entry_t)));

  cq_p = new std::queue<cq_entry_t>();

  barrier();
}

inline void finalize() {
  barrier();
  delete cq_p;
  free(gasnet_seginfo);
  finalized = true;
  gasnet_exit(0);
}

inline const size_t get_max_buffer_size() {
  return gex_AM_MaxRequestMedium(tm, GEX_RANK_INVALID, GEX_EVENT_NOW, 0, GEX_NARGS);
}

inline int send_msg(rank_t target, tag_t tag, void *buf, int nbytes) {
  CHECK_GEX(gex_AM_RequestMedium1(tm, target, gex_handler_idx,
                                  buf, nbytes, GEX_EVENT_NOW, 0, static_cast<gex_AM_Arg_t>(tag)));
  info::networkInfo.byte_send.add(nbytes);
  free(buf);
  return ARL_OK;
}

inline int poll_msg(cq_entry_t &entry) {
  ARL_Assert(cq_p != nullptr, "Didn't initialize the backend!");
  int ret = ARL_RETRY;
  if (!cq_p->empty()) {
    cq_mutex.lock();
    if (!cq_p->empty()) {
      entry = cq_p->front();
      cq_p->pop();
      info::networkInfo.byte_recv.add(entry.nbytes);
      ret = ARL_OK;
    }
    cq_mutex.unlock();
  }
  return ret;
}

inline int progress() {
  CHECK_GEX(gasnet_AMPoll());
  return ARL_RETRY;
}

inline void *buffer_alloc(int nbytes) {
  void *ptr = malloc(nbytes);
  return ptr;
}

inline void buffer_free(void *buffer) {
  free(buffer);
}

inline void broadcast(void *buf, int nbytes, rank_t root) {
  gex_Event_t event = gex_Coll_BroadcastNB(tm, root, buf, buf, nbytes, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
}

const gex_DT_t gex_datatype_map[] = {
  GEX_DT_I32,
  GEX_DT_I64,
  GEX_DT_U32,
  GEX_DT_U64,
  GEX_DT_FLT,
  GEX_DT_DBL,
};

const size_t gex_datatype_size_map[] = {
        sizeof(int32_t),
        sizeof(int64_t),
        sizeof(uint32_t),
        sizeof(uint64_t),
        sizeof(float),
        sizeof(double),
};

const gex_OP_t gex_op_map[] = {
  GEX_OP_ADD,
  GEX_OP_MULT,
  GEX_OP_MIN,
  GEX_OP_MAX,
  GEX_OP_AND,
  GEX_OP_OR,
  GEX_OP_XOR,
};

inline void reduce_one(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t, rank_t root) {
  gex_Event_t event = gex_Coll_ReduceToOneNB(
          tm, root, buf_out, buf_in,
          gex_datatype_map[static_cast<int>(datatype)], gex_datatype_size_map[static_cast<int>(datatype)], n,
          gex_op_map[static_cast<int>(op)],
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
  if (!done)
    CHECK_GEX(gasnet_AMPoll());
    return done;
  });
}

inline void reduce_all(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t) {
  gex_Event_t event = gex_Coll_ReduceToAllNB(
          tm, buf_out, buf_in,
          gex_datatype_map[static_cast<int>(datatype)], gex_datatype_size_map[static_cast<int>(datatype)], n,
          gex_op_map[static_cast<int>(op)],
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_BASE_HPP
