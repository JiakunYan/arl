#ifndef _AGGR_STORE_HPP
#define _AGGR_STORE_HPP

#define MAX_RPCS_IN_FLIGHT 8192
#define MIN_INFLIGHT_BYTES 4194304
#define ONE_MB (1024*1024)
#define ONE_GB (ONE_MB*1024)

#include <iostream>
#include <deque>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <upcxx/upcxx.hpp>

//#include "utils.hpp"

using std::vector;
using std::deque;
using std::string;
using std::ostringstream;
using std::cout;
using std::ostream;

using upcxx::intrank_t;
using upcxx::rank_me;
using upcxx::rank_n;
using upcxx::barrier;
using upcxx::dist_object;
using upcxx::rpc;
using upcxx::reduce_one;
using upcxx::reduce_all;
using upcxx::op_fast_add;
using upcxx::op_fast_max;
using upcxx::progress;
using upcxx::make_view;
using upcxx::view;
using upcxx::future;
using upcxx::make_future;
using upcxx::promise;

static string get_size_str(int64_t sz) {
  if (sz < 1024) return std::to_string(sz) + "B";
  double frac = 0;
  string units = "";
  if (sz >= ONE_GB * 1024l) {
    frac = (double)sz / (ONE_GB * 1024l);
    units = "TB";
  } else if (sz >= ONE_GB) {
    frac = (double)sz / ONE_GB;
    units = "GB";
  } else if (sz >= ONE_MB) {
    frac = (double)sz / ONE_MB;
    units = "MB";
  } else if (sz >= 1024) {
    frac = (double)sz / 1024;
    units = "KB";
  }
  ostringstream os;
  os << std::fixed << std::setprecision(2) << frac << units;
  return os.str();
}

inline void logger(ostringstream &os) {}

template <typename T, typename... Params>
inline void logger(ostringstream &os, T first, Params... params) {
  os << first;
  logger(os, params ...);
}

template <typename T, typename... Params>
inline void logger(ostream &stream, bool fail, bool serial, bool flush, T first, Params... params) {
  if (serial && upcxx::rank_me()) return;
  ostringstream os;
  os << first;
  logger(os, params ...);
  if (fail) {
    std::cerr << "\n";// << KNORM;
    throw std::runtime_error(os.str());
  }
  if (stream.rdbuf() != std::cout.rdbuf() && stream.rdbuf() != std::cerr.rdbuf()) {
    // strip out colors for log file
    string outstr = os.str();
//    for (auto c : COLORS) find_and_replace(outstr, c, "");
    stream << outstr;
  } else {
    stream << os.str();
  }
  if (flush) stream.flush();
}

#define SLOG_VERBOSE(...)

//#define SLOG_VERBOSE(...) do {                                       \
//    logger(cout, false, true, true, ##__VA_ARGS__);    \
//  } while (0)

// this class aggregates updates into local buffers and then periodically does an rpc to dispatch them

template<typename T>
class AggrStore {

  using RankStore = vector<T>;
  using Store = vector<RankStore>;
  using RpcFutures = deque< future<> >;

  Store store;
  RpcFutures rpc_futures;
  int64_t max_store_size;
  int max_rpcs_in_flight; // Limit for the number of rpcs in flight. This limit exists to prevent the dispatch buffers from growing indefinitely

  void wait_max_rpcs() {
    progress();
    while (!rpc_futures.empty() && rpc_futures.size() >= max_rpcs_in_flight) {
      rpc_futures.front().wait();
      rpc_futures.pop_front();
    }
  }

  // operate on a vector of elements in the store
  template<typename FuncDistObj, typename ...Args>
  void update_remote(intrank_t target_rank, FuncDistObj &func, Args &...args) {
    wait_max_rpcs();
    auto fut = rpc(target_rank,
                   [](FuncDistObj &func, view<T> rank_store, Args &...args) {
                     for (auto elem : rank_store) (*func)(elem, args...);
                   }, func, make_view(store[target_rank].begin(), store[target_rank].end()), args...);

    rpc_futures.push_back(fut);
    store[target_rank].clear();
  }

public:

  AggrStore()
    : store({})
    , rpc_futures()
    , max_store_size(0)
    , max_rpcs_in_flight(MAX_RPCS_IN_FLIGHT) {}

  virtual ~AggrStore() {
    clear();
  }

  void set_size(const string &desc, int64_t max_store_bytes) {
    int64_t tmp_max_rpcs_in_flight = 5 * rank_n();
    // hard maximum limit
    max_rpcs_in_flight = tmp_max_rpcs_in_flight > max_rpcs_in_flight ? max_rpcs_in_flight : tmp_max_rpcs_in_flight;
    max_store_size = max_store_bytes / (sizeof(T) * (rank_n() + max_rpcs_in_flight));
    if (max_store_size == 0) max_store_size = 1;
    store.resize(rank_n(), {});
    // reduce max in flight if necessary
    // calc remaining memory for rpcs
    int64_t tmp_inflight_bytes = max_store_bytes - (max_store_size * rank_n() * sizeof(T));
    // hard minimum limit
    int64_t max_inflight_bytes = tmp_inflight_bytes > MIN_INFLIGHT_BYTES ? tmp_inflight_bytes : MIN_INFLIGHT_BYTES;
    int64_t per_rpc_bytes = max_store_size * sizeof(T);
    if (max_rpcs_in_flight * per_rpc_bytes > max_inflight_bytes) max_rpcs_in_flight = max_inflight_bytes / per_rpc_bytes + 1;
    size_t max_target_buf = max_store_size * sizeof(T);
    SLOG_VERBOSE(desc, ": using an aggregating store for each rank of max ", get_size_str(max_store_bytes / rank_n()),
                 " per target rank\n");
    SLOG_VERBOSE("  - buffers: max ", max_store_size, " entries of ", get_size_str(sizeof(T)),
                 " per target rank (", get_size_str(max_target_buf), ")\n");
    SLOG_VERBOSE("  - buffers: max over all target ranks ", get_size_str(max_target_buf * rank_n()), "\n");
    SLOG_VERBOSE("  - RPCs in flight: max ", max_rpcs_in_flight, " RPCs of ", get_size_str(per_rpc_bytes),
                 " max per RPC (", get_size_str(max_rpcs_in_flight * per_rpc_bytes), ")\n");
    SLOG_VERBOSE("  - max possible memory: ", get_size_str(max_target_buf * rank_n() + per_rpc_bytes * max_rpcs_in_flight),
                 "\n");
    barrier();
  }

  void clear() {
    for (auto s : store) {
      if (!s.empty()) throw string("rank store is not empty!");
    }
    if (!rpc_futures.empty()) throw string("rpc_futures are not empty!");
    Store().swap(store);
    RpcFutures().swap(rpc_futures);
  }

  template<typename FuncDistObj, typename ...Args>
  void update(intrank_t target_rank, T &elem, FuncDistObj &func, Args &...args) {
    assert(max_store_size > 0);
    store[target_rank].push_back(elem);
    if (store[target_rank].size() < max_store_size) return;
    update_remote(target_rank, func, args...);
  }

  template<typename FuncDistObj, typename ...Args>
  void flush_updates(FuncDistObj &func, Args &...args) {
    for (int target_rank = 0; target_rank < rank_n(); target_rank++) {
      if (store[target_rank].size()) update_remote(target_rank, func, args...);
    }
    for (auto fut : rpc_futures) fut.wait();
    rpc_futures.clear();
    barrier();
  }

};




#endif
