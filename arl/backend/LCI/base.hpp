#ifndef ARL_BACKEND_BASE_HPP
#define ARL_BACKEND_BASE_HPP

namespace arl::backend {
inline rank_t rank_me() {
  return LCI_RANK;
}

inline rank_t rank_n() {
  return LCI_NUM_PROCESSES;
}

inline void barrier() {
  MPI_Request request;
  MPI_Ibarrier(MPI_COMM_WORLD, &request);
  progress_external_until([&](){
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
}

inline void init(uint64_t shared_segment_size) {
  MPI_Init(nullptr, nullptr);
  LCI_open();
  barrier();
}

inline void finalize() {
  barrier();
  LCI_close();
  MPI_Finalize();
}

inline const int get_max_buffer_size() {
  return LCI_MEDIUM_SIZE;
}

inline int sendm(rank_t target, tag_t tag, void *buf, int nbytes) {
  LCI_mbuffer_t src_buf;
  src_buf.address = buf;
  src_buf.length = nbytes;
  while (LCI_putmna(LCI_UR_ENDPOINT, src_buf, target, tag, LCI_UR_CQ_REMOTE) == LCI_ERR_RETRY)
    LCI_progress(LCI_UR_DEVICE);
  info::networkInfo.byte_send.add(nbytes);
  return ARL_OK;
}

inline int recvm(cq_entry_t& entry) {
  LCI_request_t request;
  LCI_error_t ret = LCI_queue_pop(LCI_UR_CQ, &request);
  if (ret == LCI_OK) {
    entry.srcRank = request.rank;
    entry.buf = request.data.mbuffer.address;
    entry.nbytes = request.data.mbuffer.length;
    entry.tag = request.tag;
    return ARL_OK;
  } else {
    return ARL_RETRY;
  }
}

inline int progress() {
  LCI_progress(LCI_UR_DEVICE);
  return ARL_OK;
}

inline void *buffer_alloc(int nbytes) {
  ARL_Assert(nbytes <= LCI_MEDIUM_SIZE);
  LCI_mbuffer_t mbuffer;
  while (LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer) == LCI_ERR_RETRY)
    progress_external();
  return mbuffer.address;
}

inline void buffer_free(void *buffer) {
  if (buffer != nullptr) {
    LCI_mbuffer_t mbuffer;
    mbuffer.address = buffer;
    LCI_mbuffer_free(mbuffer);
  }
}
} // namespace arl::backend

#endif //ARL_BACKEND_BASE_HPP
