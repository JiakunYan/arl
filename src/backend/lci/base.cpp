#include "arl_internal.hpp"

namespace arl::backend::internal {

thread_state_t thread_state;
bool to_finalize_lci = false;
bool to_finalize_mpi = false;

thread_state_t* get_thread_state() {
  return &thread_state;
}

void init() {
  // int is_mpi_initialized;
  // MPI_Initialized(&is_mpi_initialized);
  // if (!is_mpi_initialized) {
  //   int provided;
  //   MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided);
  //   if (provided != MPI_THREAD_MULTIPLE) {
  //     ARL_Error("MPI does not support MPI_THREAD_MULTIPLE");
  //   }
  //   to_finalize_mpi = true;
  // } else {
  //   int provided;
  //   MPI_Query_thread(&provided);
  //   if (provided != MPI_THREAD_MULTIPLE) {
  //     ARL_Error("MPI does not support MPI_THREAD_MULTIPLE");
  //   }
  //   to_finalize_mpi = false;
  // }

  if (!lci::is_active()) {
    lci::g_runtime_init();
    to_finalize_lci = true;
  } else {
    to_finalize_lci = false;
  }

  // assert mpi rank and lci rank are the same
  int mpi_rank;
  // MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  // if (lci::get_rank() != mpi_rank) {
  // ARL_Error("MPI rank ", mpi_rank, " does not match LCI rank ", lci::get_rank());
  // }

  thread_state.device = lci::get_default_device();
  thread_state.endpoint = lci::get_default_endpoint();
  thread_state.comp = lci::alloc_cq();
  thread_state.rcomp = lci::register_rcomp(thread_state.comp);
}

void finalize() {
  lci::free_comp(&thread_state.comp);
  if (lci::is_active() && to_finalize_lci)
    lci::g_runtime_fina();

  // int is_mpi_finalized;
  // MPI_Finalized(&is_mpi_finalized);
  // if (!is_mpi_finalized && to_finalize_mpi)
  //   MPI_Finalize();
}

}// namespace arl::backend::internal
