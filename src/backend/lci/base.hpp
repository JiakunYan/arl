#ifndef ARL_BACKEND_LCI_BASE_HPP
#define ARL_BACKEND_LCI_BASE_HPP

namespace arl::backend::internal {

struct thread_state_t {
  lci::device_t device;
  lci::endpoint_t endpoint;
  lci::comp_t cq;
  lci::rcomp_t rcomp;
};

void init(size_t custom_num_workers_per_proc,
          size_t custom_num_threads_per_proc);
void finalize();
thread_state_t *get_thread_state(bool for_progress = false);

inline rank_t rank_me() {
  return lci::get_rank();
}

inline rank_t rank_n() {
  return lci::get_nranks();
}

inline void barrier(bool (*do_something)() = nullptr) {
  if (local::rank_n() <= 1 || local::rank_me() < 0) {
    // We may be the only one, need to take care of external progress
    lci::comp_t comp = lci::alloc_sync();
    lci::barrier_x().device(get_thread_state()->device).endpoint(get_thread_state()->endpoint).comp(comp)();
    while (!lci::sync_test(comp, nullptr)) {
      progress();
      if (do_something != nullptr)
        do_something();
    }
    lci::free_comp(&comp);
  } else {
    lci::barrier_x().device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
  }
}

inline const size_t get_max_buffer_size() {
  // FIXME: Technically LCI has no limit.
  return lci::get_max_bcopy_size();
}

inline int send_msg(rank_t target, tag_t tag, void *buf, int nbytes) {
  if (config::msg_loopback && target == rank_me()) {
    lci::status_t status;
    status.error = lci::errorcode_t::ok;
    status.data = lci::buffer_t(buf, nbytes);
    status.rank = rank_me();
    status.tag = tag;
    lci::comp_signal(get_thread_state()->cq, status);
    return ARL_OK;
    
  }
  lci::status_t status;
  do {
    timer_sendmsg.start();
    status = lci::post_am_x(target, buf, nbytes, lci::COMP_NULL_EXPECT_OK_OR_RETRY, get_thread_state()->rcomp).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint).tag(tag)();
    timer_sendmsg.end();
    arl::progress_external();
  } while (status.error.is_retry());
  info::networkInfo.byte_send.add(nbytes);
  return ARL_OK;
}

inline int poll_msg(cq_entry_t &entry) {
  auto status = lci::cq_pop(get_thread_state()->cq);
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
  auto thread_state = get_thread_state(true);
  auto ret = lci::progress_x().device(thread_state->device).endpoint(thread_state->endpoint)();
  return ret.is_ok() ? ARL_OK : ARL_RETRY;
  // return ARL_RETRY;
}

inline void *buffer_alloc(int nbytes) {
  void *ret;
  do {
    ret = lci::get_upacket();
    arl::progress_external();
  } while (!ret);
  return ret;
}

inline void buffer_free(void *buffer) {
  if (!buffer) return;
  lci::put_upacket(buffer);
}

inline void broadcast(void *buf, int nbytes, rank_t root) {
  if (local::rank_n() <= 1 || local::rank_me() < 0) {
    // We may be the only one
    barrier();
  }
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
  if (local::rank_n() <= 1 || local::rank_me() < 0) {
    // We may be the only one
    barrier();
  }
  lci::reduce_x(buf_in, buf_out, n, lci_datatype_size_map[static_cast<int>(datatype)], fn, root).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
}

inline void reduce_all(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn) {
  int root = 0;
  if (local::rank_n() <= 1 || local::rank_me() < 0) {
    // We may be the only one
    barrier();
  }
  lci::reduce_x(buf_in, buf_out, n, lci_datatype_size_map[static_cast<int>(datatype)], fn, root).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
  lci::broadcast_x(buf_out, n * lci_datatype_size_map[static_cast<int>(datatype)], root).device(get_thread_state()->device).endpoint(get_thread_state()->endpoint)();
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_LCI_BASE_HPP
