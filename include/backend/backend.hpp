//
// Created by jiakunyan on 1/13/21.
//

#ifndef ARL_BACKEND_HPP
#define ARL_BACKEND_HPP

namespace arl::backend {

enum class datatype_t {
  INT32_T,
  INT64_T,
  UINT32_T,
  UINT64_T,
  FLOAT,
  DOUBLE,
};

enum class reduce_op_t {
  SUM,
  PROD,
  MIN,
  MAX,
  BAND,
  BOR,
  BXOR,
};

using reduce_fn_t = void (*)(const void *left, const void *right, void *dst,
                             size_t n);

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
extern const int get_max_buffer_size();
extern int send_msg(rank_t target, tag_t tag, void *buf, int nbytes);
extern int poll_msg(cq_entry_t &entry);
extern int progress();
extern void *buffer_alloc(int nbytes);
extern void buffer_free(void *);
extern void broadcast_impl(void *buf, int nbytes, rank_t root);
extern void reduce_one_impl(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn, rank_t root);
extern void reduce_all_impl(const void *buf_in, void *buf_out, int n, datatype_t datatype, reduce_op_t op, reduce_fn_t fn);

template<typename T>
inline T broadcast(T &val, rank_t root) {
  T rv;
  if (root == rank_me()) {
    rv = val;
  }
  broadcast_impl(&rv, sizeof(T), root);
  return rv;
}

}// namespace arl::backend

#endif//ARL_BACKEND_HPP
