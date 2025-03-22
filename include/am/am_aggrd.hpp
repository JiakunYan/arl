//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_AGGRD_HPP
#define ARL_AM_AGGRD_HPP

namespace arl {
namespace amaggrd_internal {

using am_internal::AggBuffer;
using am_internal::FutureData;
using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;

// AM synchronous counters
extern AlignedAtomicInt64 *amaggrd_ack_counter;
extern AlignedAtomicInt64 *amaggrd_req_counter;
extern AlignedInt64 *amaggrd_req_local_counters;

extern AggBuffer **amaggrd_agg_buffer_p;

struct AmaggrdReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
};

extern AmaggrdReqMeta *global_meta_p;

template<typename... Args>
struct AmaggrdReqPayload {
  intptr_t future_p;
  int target_local_rank;
  std::tuple<Args...> data;
};

struct AmaggrdAckMeta {
  intptr_t type_wrapper_p;
};

template<typename T>
struct AmaggrdAckPayload {
  intptr_t future_p;
  T data;
};

template<>
struct AmaggrdAckPayload<void> {
  intptr_t future_p;
};

template<typename Fn, typename... Args>
class AmaggrdTypeWrapper {
  public:
  static intptr_t invoker(const std::string &cmd) {
    if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else if (cmd == "ack_invoker") {
      return get_pi_fnptr(ack_invoker);
    } else {
      throw std::runtime_error("Unknown function call");
    }
  }

  static int req_invoker(intptr_t fn_p, char *buf, int nbytes, char *output, int onbytes) {
    ARL_Assert(nbytes >= (int) sizeof(ReqPayload), "(", nbytes, " >= ", sizeof(ReqPayload), ")");
    ARL_Assert(nbytes % (int) sizeof(ReqPayload) == 0, "(", nbytes, " % ", sizeof(ReqPayload), " == 0)");
    ARL_Assert(onbytes >= nbytes / sizeof(ReqPayload) * sizeof(AckPayload), "(", onbytes, " >= ", nbytes, " / ", sizeof(AckPayload), " * ", my_sizeof<AckPayload>());

    Fn *fn = resolve_pi_fnptr<Fn>(fn_p);

    for (int i = 0; i < nbytes / (int) sizeof(ReqPayload); ++i) {
      ReqPayload &payload = *reinterpret_cast<ReqPayload *>(buf + i * sizeof(ReqPayload));
      AckPayload &result = *reinterpret_cast<AckPayload *>(output + i * sizeof(AckPayload));
      result.future_p = payload.future_p;

      rank_t mContext = rank_internal::get_context();
      rank_internal::set_context(payload.target_local_rank);
      if constexpr (!std::is_void_v<Result>) {
        result.data = run_fn(fn, payload.data, std::index_sequence_for<Args...>());
      } else {
        run_fn(fn, payload.data, std::index_sequence_for<Args...>());
      }
      rank_internal::set_context(mContext);
    }
    return nbytes / (int) sizeof(ReqPayload) * (int) sizeof(AckPayload);
  }

  static int ack_invoker(char *buf, int nbytes) {
    ARL_Assert(nbytes >= (int) sizeof(AckPayload), "(", nbytes, " >= ", sizeof(AckPayload), ")");
    ARL_Assert(nbytes % (int) sizeof(AckPayload) == 0, "(", nbytes, " % ", sizeof(AckPayload), " == 0)");

    for (int i = 0; i < nbytes / (int) sizeof(AckPayload); ++i) {
      AckPayload &result = *reinterpret_cast<AckPayload *>(buf + i * sizeof(AckPayload));
      auto *future_p = reinterpret_cast<FutureData<Result> *>(result.future_p);

      if constexpr (!std::is_void_v<Result>) {
        future_p->load(result.data);
      } else {
        future_p->load();
      }
    }

    return nbytes / sizeof(AckPayload);
  }

  private:
  using ReqPayload = AmaggrdReqPayload<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;
  using AckPayload = AmaggrdAckPayload<Result>;

  template<size_t... I>
  static Result run_fn(Fn *fn, const std::tuple<Args...> &data, std::index_sequence<I...>) {
    return std::invoke(*fn, std::get<I>(data)...);
  }
};

extern void sendm_amaggrd(rank_t remote_proc, AmaggrdReqMeta meta, void *buf, size_t nbytes);
}// namespace amaggrd_internal

// TODO: handle function pointer situation
// TODO: adjust AggBuffer size
template<typename Fn, typename... Args>
void register_amaggrd(const Fn &fn) {
  pure_barrier();
  if (local::rank_me() == 0) {
    delete amaggrd_internal::global_meta_p;
    intptr_t fn_p = amaggrd_internal::get_pi_fnptr(&fn);
    intptr_t wrapper_p = amaggrd_internal::get_pi_fnptr(&amaggrd_internal::AmaggrdTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
    amaggrd_internal::global_meta_p = new amaggrd_internal::AmaggrdReqMeta{fn_p, wrapper_p};
    //    printf("register meta: %ld, %ld\n", amaggrd_internal::global_meta_p->fn_p, amaggrd_internal::global_meta_p->type_wrapper_p);
  }
  pure_barrier();
}

// TODO: handle function pointer situation
// TODO: adjust AggBuffer size
template<typename Fn, typename... Args>
void register_amaggrd(const Fn &fn, Args &&...) {
  pure_barrier();
  if (local::rank_me() == 0) {
    delete amaggrd_internal::global_meta_p;
    intptr_t fn_p = amaggrd_internal::get_pi_fnptr(&fn);
    intptr_t wrapper_p = amaggrd_internal::get_pi_fnptr(&amaggrd_internal::AmaggrdTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
    amaggrd_internal::global_meta_p = new amaggrd_internal::AmaggrdReqMeta{fn_p, wrapper_p};
    //    printf("register meta: %ld, %ld\n", amaggrd_internal::global_meta_p->fn_p, amaggrd_internal::global_meta_p->type_wrapper_p);
  }
  pure_barrier();
}

template<typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc_aggrd(rank_t remote_worker, Fn &&fn, Args &&...args) {
  using Payload = amaggrd_internal::AmaggrdReqPayload<std::remove_reference_t<Args>...>;
  using amaggrd_internal::AmaggrdReqMeta;
  using amaggrd_internal::AmaggrdTypeWrapper;

  ARL_Assert(amaggrd_internal::global_meta_p != nullptr, "call rpc_aggrd before register_aggrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  //  if (remote_proc == proc::rank_me()) {
  //    // local precedure call
  //    return amagg_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
  //  }

  Future<std::invoke_result_t<Fn, Args...>> future(true);
  Payload payload{future.get_p(), remote_worker_local, std::make_tuple(std::forward<Args>(args)...)};
  //  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
  //  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char *, int64_t> result = amaggrd_internal::amaggrd_agg_buffer_p[remote_proc]->push((char *) &payload, sizeof(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      sendm_amaggrd(remote_proc, *amaggrd_internal::global_meta_p, std::get<0>(result), std::get<1>(result));
      progress_external();
    }
  }
  ++(amaggrd_internal::amaggrd_req_local_counters[local::rank_me()].val);
  return future;
}

}// namespace arl

#endif//ARL_AM_AGGRD_HPP
