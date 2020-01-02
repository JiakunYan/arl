//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_RANK_HPP
#define ARL_RANK_HPP

namespace arl {
  // data structures
  alignas(alignof_cacheline) std::unordered_map<std::thread::id, size_t> thread_ids;
  struct ThreadContext {
    alignas(alignof_cacheline) size_t val;
  };
  alignas(alignof_cacheline) std::vector<ThreadContext> thread_contexts;
  alignas(alignof_cacheline) size_t num_threads_per_proc = 16;
  alignas(alignof_cacheline) size_t num_workers_per_proc = 15;

  // methods
  inline size_t get_thread_id() {
    return thread_ids[std::this_thread::get_id()];
  }

  inline void set_context(size_t mContext) {
    thread_contexts[get_thread_id()].val = mContext;
  }

  inline size_t get_context() {
    return thread_contexts[get_thread_id()].val;
  }

  inline size_t my_worker_local() {
    return get_context();
  }

  inline size_t nworkers_local() {
    return num_workers_per_proc;
  }

  inline size_t my_proc() {
    return backend::rank_me();
  }

  inline size_t nprocs() {
    return backend::rank_n();
  }

  inline size_t my_worker() {
    return my_worker_local() + my_proc() * num_workers_per_proc;
  }

  inline size_t nworkers() {
    return nprocs() * num_workers_per_proc;
  }
}

#endif //ARL_RANK_HPP
