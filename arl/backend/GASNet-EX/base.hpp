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

#define CHECK_GEX(x)                                                      \
  {                                                                       \
    int err = (x);                                                        \
    if (err != GASNET_OK) {                                               \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__);              \
      exit(err);                                                          \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;

namespace arl::backend {
namespace internal {
rank_t my_proc;
rank_t nprocs;
bool finalized;

gex_Client_t client;
gex_EP_t ep;
gex_TM_t tm;
const char *clientName = "ARL";

gasnet_seginfo_t *gasnet_seginfo;
uint64_t shared_segment_size;
void *smem_base_ptr;

const int GEX_NARGS = (sizeof(tag_t) + sizeof(gex_AM_Arg_t) - 1) / sizeof(gex_AM_Arg_t);
const gex_AM_Index_t gex_handler_idx = GEX_AM_INDEX_BASE;
std::queue<cq_entry_t>* cq_p;
std::mutex cq_mutex;
void gex_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes, gex_AM_Arg_t tag) {
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t srcRank = info.gex_srcrank;
  char* buf_p = new char[unbytes];
  memcpy(buf_p, void_buf, unbytes);
  cq_entry_t event_p = {
          .srcRank = srcRank,
          .tag = static_cast<tag_t>(tag),
          .buf = buf_p,
          .nbytes = static_cast<int>(unbytes)
  };
  cq_mutex.lock();
  cq_p->push(event_p);
  cq_mutex.unlock();
}
} // namespace internal

inline rank_t rank_me() {
  return internal::my_proc;
}

inline rank_t rank_n() {
  return internal::nprocs;
}

inline void barrier() {
  gex_Event_t event = gex_Coll_BarrierNB(internal::tm, 0);
  progress_external_until([&](){return !gex_Event_Test(event);});
}

inline void init(uint64_t shared_segment_size) {
  shared_segment_size = 1024*1024*shared_segment_size;

  gex_Client_Init(&internal::client, &internal::ep, &internal::tm, internal::clientName, NULL, NULL, 0);

#ifndef GASNET_PAR
    throw std::runtime_error("Need to use a par build of GASNet-EX");
#endif

  gex_Segment_t segment;
  gex_Segment_Attach(&segment, internal::tm, shared_segment_size);

  internal::smem_base_ptr = gex_Segment_QueryAddr(segment);

  if (internal::smem_base_ptr == NULL) {
    throw std::runtime_error("arl::backend: Could not allocate shared memory segment.");
  }

  internal::my_proc = gex_System_QueryJobRank();
  internal::nprocs = gex_System_QueryJobSize();

  internal::gasnet_seginfo = (gasnet_seginfo_t*) malloc(sizeof(gasnet_seginfo_t) * internal::nprocs);
  gasnet_getSegmentInfo(internal::gasnet_seginfo, rank_n());

//      init_malloc();
//      init_atomics();

  internal::finalized = false;

  gex_AM_Entry_t htable[1] = {
                { internal::gex_handler_idx,
                  (gex_AM_Fn_t) internal::gex_reqhandler,
                  GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
                  internal::GEX_NARGS } };

  CHECK_GEX(gex_EP_RegisterHandlers(internal::ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t)));

  internal::cq_p = new std::queue<cq_entry_t>();

  barrier();
}

inline void finalize() {
  barrier();
  delete internal::cq_p;
  free(internal::gasnet_seginfo);
  internal::finalized = true;
  gasnet_exit(0);
}

inline const int get_max_buffer_size() {
  return gex_AM_MaxRequestMedium(backend::internal::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,internal::GEX_NARGS);
}

inline int sendm(rank_t target, tag_t tag, void *buf, int nbytes) {
  CHECK_GEX(gex_AM_RequestMedium1(backend::internal::tm, target, internal::gex_handler_idx,
                        buf, nbytes, GEX_EVENT_NOW, 0, static_cast<gex_AM_Arg_t>(tag)));
  info::networkInfo.byte_send.add(nbytes);
  return ARL_OK;
}

inline int recvm(cq_entry_t& entry) {
  ARL_Assert(internal::cq_p != nullptr, "Didn't initialize the backend!");
  int ret = ARL_RETRY;
  if (!internal::cq_p->empty()) {
    internal::cq_mutex.lock();
    if (!internal::cq_p->empty()) {
      entry = internal::cq_p->front();
      internal::cq_p->pop();
      info::networkInfo.byte_recv.add(entry.nbytes);
      ret = ARL_OK;
    }
    internal::cq_mutex.unlock();
  }
  return ret;
}

inline int progress() {
  CHECK_GEX(gasnet_AMPoll());
  return ARL_OK;
}

inline void *buffer_alloc() {
  void *ptr = malloc(get_max_buffer_size());
  return ptr;
}

inline void buffer_free(void *buffer) {
  if (buffer != nullptr) {
    free(buffer);
  }
}
} // namespace arl::backend

#endif //ARL_BACKEND_BASE_HPP
