#ifndef ARL_HPP
#define ARL_HPP

#define ARL_THREAD_PIN
#define ARL_RPC_AS_LPC

#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <sched.h>
#include <pthread.h>
#include <utility>
#include <future>
#include <cstring>

// GASNet-EX as backend
#include <gasnetex.h>

#include "global_decl.hpp"
#include "base/op.hpp"
// tools
#include "tool/colors.hpp"
#include "tool/utils.hpp"
#include "tool/timer.hpp"
// backend
#include "backend/backend.hpp"
#include "backend/collective.hpp"
#include "backend/reduce.hpp"
// base
#include "base/detail/threadbarrier.hpp"
#include "base/rank.hpp"
#include "base/collective.hpp"
#include "base/malloc.hpp"
#include "base/base.hpp"
#include "base/worker_object.hpp"
// am
//#include "am/rpc_t.hpp"
#include "arl/am/agg_buffer.hpp"
#include "am/am.hpp"
//#include "am/am_agg.hpp"
#include "am/am_ff.hpp"
// data structure
//#include "data_structure/hash_map.hpp"
//#include "data_structure/bloom_filter.hpp"

namespace arl {
}

#endif //ARL_HPP
