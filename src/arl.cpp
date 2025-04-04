#include "arl_internal.hpp"

thread_local arl::SimpleTimer timer_sendmsg("sendmsg");
thread_local arl::SimpleTimer timer_progress("progress");
thread_local arl::SimpleTimer timer_work("work");
thread_local arl::SimpleTimer timer_backup("backup");
thread_local arl::SimpleTimer timer_collective("collective");
__thread size_t num_kmer_processed = 0;

namespace arl {
namespace rank_internal {
__thread rank_t my_rank = -1;
__thread rank_t my_context = -1;
rank_t num_threads_per_proc = 16;
rank_t num_workers_per_proc = 15;
rank_t proc_rank_me = -1;
rank_t proc_rank_n = -1;
} // namespace rank_internal

// namespace internal {
// std::vector<HandlerRegisterEntry> g_handler_registry;
// } // namespace internal

intptr_t base_fnptr = 0;

alignas(alignof_cacheline) ThreadBarrier threadBarrier;

alignas(alignof_cacheline) std::atomic<bool> thread_run = false;
alignas(alignof_cacheline) std::atomic<bool> worker_exit = false;

namespace debug {
__thread double timeout = NO_TIMEOUT;
}// namespace debug

namespace info {
__thread NetworkInfo networkInfo;
}// namespace info

void init(size_t custom_num_workers_per_proc,
          size_t custom_num_threads_per_proc) {
  char *p = getenv("ARL_NWORKERS");
  if (p) {
    custom_num_workers_per_proc = atoi(p);
    ARL_LOG(INFO, "ARL num workers per proc: %zu\n", custom_num_workers_per_proc);
  }
  p = getenv("ARL_NTHREADS");
  if (p) {
    custom_num_threads_per_proc = atoi(p);
    ARL_LOG(INFO, "ARL num threads per proc: %zu\n", custom_num_threads_per_proc);
  }
  config::init();

#ifdef ARL_DEBUG
  proc::print("%s", "WARNING: Running low-performance debug mode.\n");
#endif
  rank_internal::num_workers_per_proc = custom_num_workers_per_proc;
  rank_internal::num_threads_per_proc = custom_num_threads_per_proc;
  backend::init(custom_num_workers_per_proc, custom_num_threads_per_proc);
  rank_internal::proc_rank_me = backend::rank_me();
  rank_internal::proc_rank_n = backend::rank_n();
  threadBarrier.init(local::rank_n());

  am_internal::init_am();
}

void finalize() {
  am_internal::exit_am();
  backend::finalize();
}
}// namespace arl