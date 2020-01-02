#ifndef ARL_HPP
#define ARL_HPP

#define ARL_THREAD_PIN

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

// GASNet-EX as backend
#include <gasnetex.h>

#include "global_decl.hpp"
// tools
#include "tool/assert.hpp"
#include "tool/timer.hpp"
// backend
#include "backend/backend.hpp"
#include "backend/collective.hpp"
// base
#include "base/detail/threadbarrier.hpp"
#include "base/rank.hpp"
#include "base/base.hpp"
#include "base/collective.hpp"
#include "base/worker_object.hpp"
// am
#include "am/rpc_t.hpp"
#include "am/am.hpp"
#include "am/agg_buffer.hpp"
#include "am/am_agg.hpp"
// data structure
#include "data_structure/hash_map.hpp"

namespace arl {
}

#endif //ARL_HPP
