//
// Created by Jiakun Yan on 11/12/19.
//

#ifndef ARL_GLOBAL_HPP
#define ARL_GLOBAL_HPP

namespace arl {

using rank_t = int64_t;
using tick_t = int64_t;

inline const int alignof_cacheline = 64;

struct AlignedAtomicInt64 {
  alignas(alignof_cacheline) std::atomic<int64_t> val;
};

struct AlignedInt64 {
  alignas(alignof_cacheline) int64_t val;
};
}

#endif //ARL_GLOBAL_HPP
