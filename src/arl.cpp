#include "arl_internal.hpp"

namespace arl {
namespace rank_internal {
__thread rank_t my_rank = -1;
__thread rank_t my_context = -1;
rank_t num_threads_per_proc = 16;
rank_t num_workers_per_proc = 15;
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
  config::init();
  backend::init();

#ifdef ARL_DEBUG
  proc::print("%s", "WARNING: Running low-performance debug mode.\n");
#endif
#ifndef ARL_THREAD_PIN
  proc::print("%s", "WARNING: Haven't pinned threads to cores.\n");
#endif
  rank_internal::num_workers_per_proc = custom_num_workers_per_proc;
  rank_internal::num_threads_per_proc = custom_num_threads_per_proc;
  threadBarrier.init(local::rank_n());

  am_internal::init_am();
}

void finalize() {
  am_internal::exit_am();
  backend::finalize();
}
}// namespace arl