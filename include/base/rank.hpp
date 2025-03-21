//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_RANK_HPP
#define ARL_RANK_HPP

namespace arl {
namespace rank_internal {
extern __thread rank_t my_rank;
extern __thread rank_t my_context;
extern rank_t num_threads_per_proc;
extern rank_t num_workers_per_proc;

inline void set_context(rank_t mContext) {
  my_context = mContext;
}

inline rank_t get_context() {
  return my_context;
}
}// namespace rank_internal

namespace local {
inline rank_t rank_me() {
  return rank_internal::get_context();
}

inline rank_t rank_n() {
  return rank_internal::num_workers_per_proc;
}

inline rank_t thread_n() {
  return rank_internal::num_threads_per_proc;
}
}// namespace local

namespace proc {
inline rank_t rank_me() {
  return backend::rank_me();
}

inline rank_t rank_n() {
  return backend::rank_n();
}
}// namespace proc

inline rank_t rank_me() {
  return local::rank_me() + proc::rank_me() * rank_internal::num_workers_per_proc;
}

inline rank_t rank_n() {
  return proc::rank_n() * rank_internal::num_workers_per_proc;
}
}// namespace arl

#endif//ARL_RANK_HPP
