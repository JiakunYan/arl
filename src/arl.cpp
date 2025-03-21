#include "arl_internal.hpp"

namespace arl {
__thread rank_t my_rank = -1;
__thread rank_t my_context = -1;
rank_t num_threads_per_proc = 16;
rank_t num_workers_per_proc = 15;

namespace debug {
__thread double timeout = NO_TIMEOUT;
}// namespace debug

namespace info {
__thread NetworkInfo networkInfo;
}// namespace info
}// namespace arl