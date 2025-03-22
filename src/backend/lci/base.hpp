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

inline void broadcast(void *buf, int nbytes, rank_t root) {
  MPI_Request request;
  MPI_Ibcast(buf, nbytes, MPI_BYTE, root, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
}

const MPI_Datatype mpi_datatype_map[] = {
  MPI_INT32_T,
  MPI_INT64_T,
  MPI_UINT32_T,
  MPI_UINT64_T,
  MPI_FLOAT,
  MPI_DOUBLE,
};

const MPI_Op mpi_op_map[] = {
  MPI_SUM,
  MPI_PROD,
  MPI_MIN,
  MPI_MAX,
  MPI_BAND,
  MPI_BOR,
  MPI_BXOR,
};

inline void reduce_one(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, rank_t root) {
  MPI_Request request;
  MPI_Ireduce(buf_in, buf_out, n, mpi_datatype_map[static_cast<int>(datatype)], mpi_op_map[static_cast<int>(op)], root, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
}

inline void reduce_all(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op) {
  MPI_Request request;
  MPI_Iallreduce(buf_in, buf_out, n, mpi_datatype_map[static_cast<int>(datatype)], mpi_op_map[static_cast<int>(op)], MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_LCI_BASE_HPP
