#include "arl_internal.hpp"

namespace arl::backend {
rank_t rank_me() {
  return internal::rank_me();
}
rank_t rank_n() {
  return internal::rank_n();
}
void barrier() {
  internal::barrier();
}
void init() {
  internal::init();
}
void finalize() {
  internal::finalize();
}
template<typename T>
T broadcast(T &val, rank_t root) {
  return internal::broadcast(val, root);
}
template<typename T, typename BinaryOp>
T reduce_one(const T &value, const BinaryOp &op, rank_t root) {
  return internal::reduce_one(value, op, root);
}
template<typename T, typename BinaryOp>
std::vector<T> reduce_one(const std::vector<T> &value, const BinaryOp &op, rank_t root) {
  return internal::reduce_one(value, op, root);
}
template<typename T, typename BinaryOp>
T reduce_all(const T &value, const BinaryOp &op) {
  return internal::reduce_all(value, op);
}
template<typename T, typename BinaryOp>
std::vector<T> reduce_all(const std::vector<T> &value, const BinaryOp &op) {
  return internal::reduce_all(value, op);
}
const int get_max_buffer_size() {
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
}// namespace arl::backend