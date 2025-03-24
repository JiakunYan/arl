#ifndef ARL_BACKEND_LCI_BASE_HPP
#define ARL_BACKEND_LCI_BASE_HPP

namespace arl::backend::internal {

struct thread_state_t {
  lci::device_t device;
  lci::endpoint_t endpoint;
  lci::comp_t comp;
  lci::rcomp_t rcomp;
};

void init();
void finalize();
thread_state_t *get_thread_state();

inline rank_t rank_me() {
  return lci::get_rank();
}

inline rank_t rank_n() {
  return lci::get_nranks();
}

inline void barrier() {
  lci::comp_t comp = lci::alloc_sync();
  lci::barrier_x().device(get_thread_state()->device).endpoint(get_thread_state()->endpoint).comp(comp)();
  while (!lci::sync_test(comp, nullptr)) {
    arl::progress_external();
    arl::progress_internal();
  }
  lci::free_comp(&comp);
}

inline const int get_max_buffer_size() {
  // FIXME: Technically LCI has no limit.
  return lci::get_max_bcopy_size();
}

inline int send_msg(rank_t target, tag_t tag, void *buf, int nbytes) {
  lci::post_am_x(target, buf, nbytes, lci::COMP_NULL_EXPECT_OK, get_thread_state()->rcomp).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint).tag(tag)();
  info::networkInfo.byte_send.add(nbytes);
  return ARL_OK;
}

inline int poll_msg(cq_entry_t &entry) {
  auto status = lci::cq_pop(get_thread_state()->comp);
  if (status.error.is_ok()) {
    entry.srcRank = status.rank;
    entry.tag = status.tag;
    auto buffer = status.data.get_buffer();
    entry.buf = buffer.base;
    entry.nbytes = buffer.size;
    return ARL_OK;
  } else {
    return ARL_RETRY;
  }
}

inline int progress() {
  auto ret = lci::progress_x().device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
  return ret.is_ok() ? ARL_OK : ARL_RETRY;
}

inline void *buffer_alloc(int nbytes) {
  void *buffer;
  int ret = posix_memalign(&buffer, 8192, nbytes);
  if (ret != 0) {
    // Allocation failed
    throw std::runtime_error("posix_memalign failed\n");
  }
  return buffer;
}

inline void buffer_free(void *buffer) {
  free(buffer);
}

inline void broadcast(void *buf, int nbytes, rank_t root) {
  barrier();
  lci::broadcast_x(buf, nbytes, root).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
}

const size_t lci_datatype_size_map[] = {
        sizeof(int32_t),
        sizeof(int64_t),
        sizeof(uint32_t),
        sizeof(uint64_t),
        sizeof(float),
        sizeof(double),
};

inline void reduce_one(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn, rank_t root) {
  barrier();
  lci::reduce_x(buf_in, buf_out, n, lci_datatype_size_map[static_cast<int>(datatype)], fn, root).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
}

inline void reduce_all(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn) {
  int root = 0;
  reduce_one(buf_in, buf_out, n, datatype, op, fn, root);
  broadcast(buf_out, n * lci_datatype_size_map[static_cast<int>(datatype)], root);
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_LCI_BASE_HPP
