//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AMFF_HPP
#define ARL_AMFF_HPP

namespace arl {
namespace amff_internal {

using am_internal::AggBuffer;
using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;

// AM synchronous counters
extern AlignedAtomicInt64 *amff_recv_counter;
extern AlignedAtomicInt64 *amff_req_counters;// of length proc::rank_n()

extern AggBuffer **amff_agg_buffer_p;

struct AmffReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
  int target_local_rank;
};

template<typename Fn, typename... Args>
class AmffTypeWrapper {
  public:
  static intptr_t invoker(const std::string &cmd) {
    if (cmd == "payload_size") {
      return get_pi_fnptr(payload_size);
    } else if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else {
      printf("cmd: %s\n", cmd.c_str());
      throw std::runtime_error("Unknown function call");
    }
  }

  static constexpr int payload_size() {
    return sizeof(Payload);
  }

  static void req_invoker(intptr_t fn_p, int context, char *buf, int nbytes) {
    static_assert(std::is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    Fn *fn = resolve_pi_fnptr<Fn>(fn_p);
    Payload *ptr = reinterpret_cast<Payload *>(buf);

    rank_t mContext = rank_internal::get_context();
    rank_internal::set_context(context);
    run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    rank_internal::set_context(mContext);
  }

  private:
  using Payload = std::tuple<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template<size_t... I>
  static void run_fn(Fn *fn, const Payload &data, std::index_sequence<I...>) {
    std::invoke(*fn, std::get<I>(data)...);
  }
};

template<typename Fn, typename... Args>
void run_lpc(rank_t context, Fn &&fn, Args &&...args) {
  rank_t mContext = rank_internal::get_context();
  rank_internal::set_context(context);
  std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
  rank_internal::set_context(mContext);
}
}// namespace amff_internal

template<typename Fn, typename... Args>
void rpc_ff(rank_t remote_worker, Fn &&fn, Args &&...args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amff_internal::AmffReqMeta;
  using amff_internal::AmffTypeWrapper;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  if (config::rpc_loopback && remote_proc == proc::rank_me()) {
    // local precedure call
    amff_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
    return;
  }

  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmffTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmffReqMeta meta{fn_p, wrapper_p, remote_worker_local};
  Payload payload(std::forward<Args>(args)...);

  //  std::pair<am_internal::AmffReqMeta, Payload> data{meta, payload};
  //  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
  //  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char *, int64_t> result = amff_internal::amff_agg_buffer_p[remote_proc]->push((char *) &meta, sizeof(meta), (char *) &payload, sizeof(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      backend::send_msg(remote_proc, am_internal::HandlerType::AM_FF_REQ, std::get<0>(result), std::get<1>(result));
      progress_external();
    }
  }
  ++(amff_internal::amff_req_counters[remote_proc].val);
}

}// namespace arl

#endif//ARL_AMFF_HPP
