//
// Created by jiakunyan on 1/13/21.
//

#ifndef ARL_BACKEND_HPP
#define ARL_BACKEND_HPP

namespace arl::backend {
// Active message handler
using tag_t = uint16_t;

struct cq_entry_t {
  rank_t srcRank;
  tag_t tag;
  void *buf;
  int nbytes;
};

extern rank_t rank_me();
extern rank_t rank_n();
extern void barrier();
extern void init();
extern void finalize();
template<typename T>
extern T broadcast(T &val, rank_t root);
template<typename T, typename BinaryOp>
extern T reduce_one(const T &value, const BinaryOp &op, rank_t root);
template<typename T, typename BinaryOp>
extern std::vector<T> reduce_one(const std::vector<T> &value, const BinaryOp &op, rank_t root);
template<typename T, typename BinaryOp>
extern T reduce_all(const T &value, const BinaryOp &op);
template<typename T, typename BinaryOp>
extern std::vector<T> reduce_all(const std::vector<T> &value, const BinaryOp &op);
extern const int get_max_buffer_size();
extern int send_msg(rank_t target, tag_t tag, void *buf, int nbytes);
extern int poll_msg(cq_entry_t &entry);
extern int progress();
extern void *buffer_alloc(int nbytes);
extern void buffer_free(void *);
}// namespace arl::backend

#endif//ARL_BACKEND_HPP
