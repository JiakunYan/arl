//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_RANK_HPP
#define ARL_RANK_HPP

namespace arl {
namespace rank_internal {
__thread rank_t my_rank = -1;
__thread rank_t my_context = -1;
alignas(alignof_cacheline) rank_t num_threads_per_proc = 16;
alignas(alignof_cacheline) rank_t num_workers_per_proc = 15;

inline void set_context(rank_t mContext) {
  my_context = mContext;
}

inline rank_t get_context() {
  return my_context;
}
} // namespace rank_internal

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
}

namespace proc {
    inline rank_t rank_me() {
      return backend::rank_me();
    }

    inline rank_t rank_n() {
      return backend::rank_n();
    }
}

inline rank_t rank_me() {
  return local::rank_me() + proc::rank_me() * rank_internal::num_workers_per_proc;
}

inline rank_t rank_n() {
  return proc::rank_n() * rank_internal::num_workers_per_proc;
}
} // namespace arl

#endif //ARL_RANK_HPP
