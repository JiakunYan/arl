//
// Created by Jiakun Yan on 11/12/19.
//

#ifndef ARL_GLOBAL_HPP
#define ARL_GLOBAL_HPP

namespace arl {

using rank_t = int64_t;
using tick_t = int64_t;

inline const int alignof_cacheline = 64;
inline const double NO_TIMEOUT = 0;

struct AlignedAtomicInt64 {
  alignas(alignof_cacheline) std::atomic<int64_t> val;
};

struct AlignedInt64 {
  alignas(alignof_cacheline) int64_t val;
};

enum RPCType {
  RPC_AGG = 0x1,
  RPC_FF = 0x1 << 1,
  RPC_AGGRD = 0x1 << 2,
  RPC_FFRD = 0x1 << 3
};

void flush_agg_buffer(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
void wait_am(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
void flush_am(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
bool progress(void);
void progress_until(const std::function<bool()>&); // with deadlock detector
extern rank_t rank_me();
extern rank_t rank_n();

namespace proc {
extern rank_t rank_me();
extern rank_t rank_n();
} // namespace proc

namespace local {
extern rank_t rank_me();
extern rank_t rank_n();
} // namespace local

namespace amaggrd_internal {
extern int64_t get_amaggrd_buffer_size();
}

namespace debug {
__thread double timeout = NO_TIMEOUT;
}
} // namespace arl

#endif //ARL_GLOBAL_HPP
