#include "arl_internal.hpp"

namespace arl::backend::internal {

thread_state_t thread_state;

thread_state_t* get_thread_state() {
  return &thread_state;
}

void init() {
  lci::g_runtime_init_x();
  thread_state.device = lci::get_default_device();
  thread_state.endpoint = lci::get_default_endpoint();
  thread_state.comp = lci::alloc_cq();
  thread_state.rcomp = lci::register_rcomp(thread_state.comp);
}

void finalize() {
  lci::free_comp(&thread_state.comp);
  lci::g_runtime_fina();
}

template<>
const MPI_Datatype reduce_mpi_ty_id_integral<32, /*signed=*/true>::ty_id = MPI_INT32_T;
template<>
const MPI_Datatype reduce_mpi_ty_id_integral<64, /*signed=*/true>::ty_id = MPI_INT64_T;
template<>
const MPI_Datatype reduce_mpi_ty_id_integral<32, /*signed=*/false>::ty_id = MPI_UINT32_T;
template<>
const MPI_Datatype reduce_mpi_ty_id_integral<64, /*signed=*/false>::ty_id = MPI_UINT64_T;
template<>
const MPI_Datatype reduce_mpi_ty_id_floating<32>::ty_id = MPI_FLOAT;
template<>
const MPI_Datatype reduce_mpi_ty_id_floating<64>::ty_id = MPI_DOUBLE;

template<>
const MPI_Op reduce_mpi_op_id<op_plus>::op_id = MPI_SUM;
template<>
const MPI_Op reduce_mpi_op_id<op_multiplies>::op_id = MPI_PROD;
template<>
const MPI_Op reduce_mpi_op_id<op_min>::op_id = MPI_MIN;
template<>
const MPI_Op reduce_mpi_op_id<op_max>::op_id = MPI_MAX;
template<>
const MPI_Op reduce_mpi_op_id<op_bit_and>::op_id = MPI_BAND;
template<>
const MPI_Op reduce_mpi_op_id<op_bit_or>::op_id = MPI_BOR;
template<>
const MPI_Op reduce_mpi_op_id<op_bit_xor>::op_id = MPI_BXOR;

}// namespace arl::backend::internal
