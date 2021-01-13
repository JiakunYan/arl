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
  void* buf;
  int nbytes;
};

extern inline rank_t rank_me();
extern inline rank_t rank_n();
extern inline void barrier();
extern inline void init(uint64_t shared_segment_size);
extern inline void finalize();
template <typename T>
extern inline T broadcast(T& val, rank_t root);
template<typename T, typename BinaryOp>
extern inline T reduce_one(const T& value, const BinaryOp& op, rank_t root);
template<typename T, typename BinaryOp>
extern inline std::vector<T> reduce_one(const std::vector<T>& value, const BinaryOp& op, rank_t root);
template<typename T, typename BinaryOp>
extern inline T reduce_all(const T& value, const BinaryOp& op);
template<typename T, typename BinaryOp>
extern inline std::vector<T> reduce_all(const std::vector<T>& value, const BinaryOp& op);
extern inline const int get_max_buffer_size();
extern inline int sendm(rank_t target, tag_t tag, void *buf, int nbytes);
extern inline int recvm(cq_entry_t& entry);
extern inline int progress();
} // namespace arl::backend

#ifdef ARL_USE_GEX
#include <gasnetex.h>
#include "GASNet-EX/base.hpp"
#include "GASNet-EX/collective.hpp"
#include "GASNet-EX/reduce.hpp"
#endif

#endif //ARL_BACKEND_HPP
