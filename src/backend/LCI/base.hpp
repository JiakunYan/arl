#ifndef ARL_BACKEND_BASE_HPP
#define ARL_BACKEND_BASE_HPP

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
  lci::barrier_x().device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
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
}// namespace arl::backend::internal

#endif//ARL_BACKEND_BASE_HPP
