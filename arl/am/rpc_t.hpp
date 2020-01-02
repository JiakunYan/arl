#ifndef ARL_RPC_T_HPP
#define ARL_RPC_T_HPP

#include <array>
#define CHECK_SIZE 0 // compiler trick: be used to check payload size

namespace arl {
  extern void init(size_t, size_t, size_t);

  // Get a *position independent* function pointer
  template<typename T>
  std::uintptr_t get_pi_fnptr_(T *fn) {
    return reinterpret_cast<std::uintptr_t>(fn) - reinterpret_cast<std::uintptr_t>(init);
  }

  // Resolve a *position independent* function pointer
  // to a local function pointer.
  template<typename T = char>
  T *resolve_pi_fnptr_(std::uintptr_t fn) {
    return reinterpret_cast<T *>(fn + reinterpret_cast<std::uintptr_t>(init));
  }

  struct FutureData;

  struct result_t {
    static constexpr size_t max_payload_size = 24; // return value data size
    using payload_t = std::array<char, max_payload_size>;

    FutureData* future_p_; // 8 Bytes
    payload_t data_; // 8 Bytes
    u_int8_t source_worker_local_; // 1 Byte

    result_t(FutureData* future_p, u_int8_t source_worker_local) : future_p_(future_p), source_worker_local_(source_worker_local) {}

    template<typename T>
    void load_result(const T &value) {
#if CHECK_SIZE
      char (*__sizeof_result_payload__)[sizeof( T )] = 1;
#endif
      static_assert(sizeof(T) <= max_payload_size, "Max return val size too small");
      *reinterpret_cast<T *>(data_.data()) = value;
    }
  };

  struct FutureData {
    using payload_t = result_t::payload_t;
    std::atomic<bool> ready;
    payload_t payload;

    FutureData(): ready(false) {}
  };

  template<typename Fn, typename... Args>
  struct rpc_invoker;

  struct rpc_t {
    static constexpr size_t max_payload_size = 40; // argument value data size
    using payload_t = std::array<char, max_payload_size>;
    using rpc_result_t = result_t;

    FutureData* future_p_; // 8 Bytes
    std::uintptr_t fn_; // 8 Bytes
    std::uintptr_t invoker_; // 8 Bytes
    payload_t data_; // 8 Bytes
    u_int8_t source_worker_local_; // 1 Byte
    u_int8_t target_worker_local_; // 1 Byte

    rpc_t(FutureData* future_p, u_int8_t target_worker_local) :
      future_p_(future_p), source_worker_local_((u_int8_t)my_worker_local()), target_worker_local_(target_worker_local) {}
    rpc_t() = default;
    rpc_t(rpc_t&& t) = default;
    rpc_t& operator=(rpc_t&& t) = default;

    template<typename Fn, typename... Args>
    void load(Fn &&fn, Args &&... args) {
      using tuple_t = std::tuple<std::remove_reference_t<Args>...>;
#if CHECK_SIZE
      char (*__sizeof_rpc_payload__)[sizeof( tuple_t )] = 1;
#endif
      static_assert(sizeof(tuple_t) <= max_payload_size, "Max RPC size too small.");

      this->fn_ = get_pi_fnptr_(reinterpret_cast<char *>(+fn));

      invoker_ = get_pi_fnptr_(
              reinterpret_cast<char *>(&decltype(
              rpc_invoker<Fn, Args...>())::invoke));

      tuple_t &tuple = *reinterpret_cast<tuple_t *>(data_.data());
      new(&tuple) tuple_t();

      tuple = std::tie(args...);
    }

    rpc_result_t run() {
      using invoker_type = rpc_result_t (*)(rpc_t &);

      auto invoker = reinterpret_cast<invoker_type>
              (resolve_pi_fnptr_(invoker_));
      rpc_result_t rpc_result = invoker(*this);
      return rpc_result;
    }
  };

  template<typename Fn, typename... Args>
  struct rpc_invoker {
    using rpc_result_t = rpc_t::rpc_result_t;

    template<std::size_t... I>
    static auto invoke_impl_(rpc_t &rpc_requests, std::index_sequence<I...>) {
      using fn_t = decltype(+std::declval<std::remove_reference_t<Fn>>());
      fn_t fn = reinterpret_cast<fn_t>(resolve_pi_fnptr_(rpc_requests.fn_));
      using tuple_t = std::tuple<std::remove_reference_t<Args>...>;
      tuple_t &args_ptr = *reinterpret_cast<tuple_t *>(rpc_requests.data_.data());

      return std::invoke(std::forward<fn_t>(fn),
                         std::forward<Args>(std::get<I>(args_ptr))...);
    }

    static rpc_result_t invoke(rpc_t &rpc_requests) {
      rpc_result_t rpc_result(rpc_requests.future_p_, rpc_requests.source_worker_local_);
      if constexpr(std::is_void<std::invoke_result_t<Fn, Args...>>::value) {
        invoke_impl_(rpc_requests, std::index_sequence_for<Args...>{});
      } else {
        rpc_result.load_result(invoke_impl_(rpc_requests, std::index_sequence_for<Args...>{}));
      }
      return rpc_result;
    }
  };
}
#undef CHECK_SIZE
#endif //ARL_RPC_T_HPP
