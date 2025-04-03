//
// Created by Jiakun Yan on 11/12/19.
//

#ifndef ARL_GLOBAL_HPP
#define ARL_GLOBAL_HPP

namespace arl {

#define Register_Op(name, fun)                                             \
  struct name {                                                            \
    template<class T, class U>                                             \
    constexpr auto operator()(T &&lhs, U &&rhs) const                      \
            -> decltype(fun(std::forward<T>(lhs), std::forward<U>(rhs))) { \
      return fun(std::forward<T>(lhs), std::forward<U>(rhs));              \
    }                                                                      \
  };

Register_Op(op_plus, std::plus<>{})
        Register_Op(op_multiplies, std::multiplies<>{})
                Register_Op(op_bit_and, std::bit_and<>{})
                        Register_Op(op_bit_or, std::bit_or<>{})
                                Register_Op(op_bit_xor, std::bit_xor<>{})
                                        Register_Op(op_min, std::min)
                                                Register_Op(op_max, std::max)

#undef Register_Op

#define ARL_LOG(lvl, ...)                                              \
  do {                                                                 \
    if (::arl::config::log_level >= ::arl::config::log_level_t::lvl) { \
      fprintf(stderr, __VA_ARGS__);                                    \
    }                                                                  \
  } while (0)

                                                        const int ARL_OK = 0;
const int ARL_RETRY = -1;
const int alignof_cacheline = 64;
const double NO_TIMEOUT = 0;

using rank_t = int64_t;
using tick_t = int64_t;

struct AlignedAtomicInt64 {
  alignas(alignof_cacheline) std::atomic<int64_t> val;
  char padding[alignof_cacheline - sizeof(std::atomic<int64_t>)];
};

struct AlignedInt64 {
  alignas(alignof_cacheline) int64_t val;
  char padding[alignof_cacheline - sizeof(int64_t)];
};

enum RPCType {
  RPC_AGG = 0x1,
  RPC_FF = 0x1 << 1,
  RPC_AGGRD = 0x1 << 2,
  RPC_FFRD = 0x1 << 3
};

extern void flush_agg_buffer(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
extern void wait_am(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
extern void flush_am(char rpc_type = RPC_AGG | RPC_FF | RPC_AGGRD | RPC_FFRD);
inline bool progress_internal();
inline bool progress_external();
extern void progress_external_until(const std::function<bool()> &);// with deadlock detector
extern rank_t rank_me();
extern rank_t rank_n();

namespace proc {
extern rank_t rank_me();
extern rank_t rank_n();
template<typename... Args>
extern void print(const std::string &format, Args... args);
}// namespace proc

namespace local {
extern rank_t rank_me();
extern rank_t rank_n();
}// namespace local

namespace amaggrd_internal {
extern int64_t get_amaggrd_buffer_size();
extern std::string get_amaggrd_buffer_status();
}// namespace amaggrd_internal

namespace debug {
extern __thread double timeout;
}
}// namespace arl

#endif//ARL_GLOBAL_HPP
