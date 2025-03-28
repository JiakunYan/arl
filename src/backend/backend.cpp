#include "arl_internal.hpp"

namespace arl::backend {
rank_t rank_me() {
  return internal::rank_me();
}
rank_t rank_n() {
  return internal::rank_n();
}
void barrier(bool (*do_something)()) {
  internal::barrier(do_something);
}
void init(size_t custom_num_workers_per_proc,
          size_t custom_num_threads_per_proc) {
  internal::init(custom_num_workers_per_proc, custom_num_threads_per_proc);
}
void finalize() {
  internal::finalize();
}
const size_t get_max_buffer_size() {
  return internal::get_max_buffer_size();
}
int send_msg(rank_t target, tag_t tag, void *buf, int nbytes) {
  return internal::send_msg(target, tag, buf, nbytes);
}
int poll_msg(cq_entry_t &entry) {
  return internal::poll_msg(entry);
}
int progress() {
  return internal::progress();
}
void *buffer_alloc(int nbytes) {
  return internal::buffer_alloc(nbytes);
}
void buffer_free(void *buffer) {
  internal::buffer_free(buffer);
}
void broadcast_impl(void *buf, int nbytes, rank_t root) {
  internal::broadcast(buf, nbytes, root);
}
void reduce_one_impl(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn, rank_t root) {
  internal::reduce_one(buf_in, buf_out, n, datatype, op, fn, root);
}
void reduce_all_impl(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn) {
  internal::reduce_all(buf_in, buf_out, n, datatype, op, fn);
}

template<>
const datatype_t reduce_ty_id_integral<32, /*signed=*/true>::ty_id = datatype_t::INT32_T;
template<>
const datatype_t reduce_ty_id_integral<64, /*signed=*/true>::ty_id = datatype_t::INT64_T;
template<>
const datatype_t reduce_ty_id_integral<32, /*signed=*/false>::ty_id = datatype_t::UINT32_T;
template<>
const datatype_t reduce_ty_id_integral<64, /*signed=*/false>::ty_id = datatype_t::UINT64_T;
template<>
const datatype_t reduce_ty_id_floating<32>::ty_id = datatype_t::FLOAT;
template<>
const datatype_t reduce_ty_id_floating<64>::ty_id = datatype_t::DOUBLE;

template<>
const reduce_op_t reduce_op_id<op_plus>::op_id = reduce_op_t::SUM;
template<>
const reduce_op_t reduce_op_id<op_multiplies>::op_id = reduce_op_t::PROD;
template<>
const reduce_op_t reduce_op_id<op_min>::op_id = reduce_op_t::MIN;
template<>
const reduce_op_t reduce_op_id<op_max>::op_id = reduce_op_t::MAX;
template<>
const reduce_op_t reduce_op_id<op_bit_and>::op_id = reduce_op_t::BAND;
template<>
const reduce_op_t reduce_op_id<op_bit_or>::op_id = reduce_op_t::BOR;
template<>
const reduce_op_t reduce_op_id<op_bit_xor>::op_id = reduce_op_t::BXOR;
}// namespace arl::backend