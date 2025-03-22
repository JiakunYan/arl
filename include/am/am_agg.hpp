//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_AGG_HPP
#define ARL_AM_AGG_HPP

namespace arl {
namespace amagg_internal {

using am_internal::AggBuffer;
using am_internal::FutureData;
using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;

// AM synchronous counters
extern AlignedAtomicInt64 *amagg_ack_counter;
extern AlignedAtomicInt64 *amagg_req_counter;
extern AlignedInt64 *amagg_req_local_counters;

extern AggBuffer **amagg_agg_buffer_p;

struct AmaggReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
  intptr_t future_p;
  int target_local_rank;
};

struct AmaggAckMeta {
  intptr_t future_p;
  intptr_t type_wrapper_p;
};

template<typename Fn, typename... Args>
class AmaggTypeWrapper {
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

  static std::pair<int, int> req_invoker(intptr_t fn_p, int context, char *buf, int nbytes, char *output, int onbytes) {
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    if constexpr (!std::is_void_v<Result>)
      ARL_Assert(onbytes >= (int) my_sizeof<Result>(), "(", onbytes, " >= ", my_sizeof<Result>(), ")");

    Fn *fn = resolve_pi_fnptr<Fn>(fn_p);
    auto *ptr = reinterpret_cast<Payload *>(buf);

    rank_t mContext = rank_internal::get_context();
    rank_internal::set_context(context);
    if constexpr (!std::is_void_v<Result>)
      *reinterpret_cast<Result *>(output) = run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    else
      run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    rank_internal::set_context(mContext);
    return std::make_pair(sizeof(Payload), my_sizeof<Result>());
  }

  static int ack_invoker(intptr_t raw_future_p, char *buf, int nbytes) {
    ARL_Assert(nbytes >= (int) my_sizeof<Result>(), "(", nbytes, " >= ", my_sizeof<Result>(), ")");
    auto *future_p = reinterpret_cast<FutureData<Result> *>(raw_future_p);

    if constexpr (!std::is_void_v<Result>) {
      auto *ptr = reinterpret_cast<Result *>(buf);
      future_p->load(*ptr);
    } else {
      future_p->load();
    }
    //    future_p->set_ready();

    return my_sizeof<Result>();
  }

  private:
  using Payload = std::tuple<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template<size_t... I>
  static Result run_fn(Fn *fn, const Payload &data, std::index_sequence<I...>) {
    return std::invoke(*fn, std::get<I>(data)...);
  }
};

template<typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> run_lpc(rank_t context, Fn &&fn, Args &&...args) {
  using Result = std::invoke_result_t<Fn, Args...>;

  Future<Result> future(true);
  auto *future_p = reinterpret_cast<FutureData<Result> *>(future.get_p());

  rank_t mContext = rank_internal::get_context();
  rank_internal::set_context(context);
  if constexpr (!std::is_void_v<Result>) {
    auto result = std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
    rank_internal::set_context(mContext);
    future_p->load(result);
  } else {
    std::invoke(fn, args...);
    rank_internal::set_context(mContext);
    future_p->load();
  }

  return future;
}
}// namespace amagg_internal

template<typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc(rank_t remote_worker, Fn &&fn, Args &&...args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amagg_internal::AmaggReqMeta;
  using amagg_internal::AmaggTypeWrapper;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();

  Future<std::invoke_result_t<Fn, Args...>> future(true);
  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmaggTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmaggReqMeta meta{fn_p, wrapper_p, future.get_p(), remote_worker_local};
  Payload payload(std::forward<Args>(args)...);
  //  printf("send meta: %ld, %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.future_p, meta.target_local_rank);
  //  printf("sizeof(payload): %lu\n", sizeof(Payload));

  char *ptr = (char *) backend::buffer_alloc(sizeof(AmaggReqMeta) + sizeof(Payload));
  *reinterpret_cast<AmaggReqMeta *>(ptr) = meta;
  *reinterpret_cast<Payload *>(ptr + sizeof(AmaggReqMeta)) = payload;
  backend::send_msg(remote_proc, am_internal::HandlerType::AM_REQ, ptr, sizeof(AmaggReqMeta) + sizeof(Payload));
  progress_external();
  ++amagg_internal::amagg_req_local_counters[local::rank_me()].val;
  return future;
}

template<typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc_agg(rank_t remote_worker, Fn &&fn, Args &&...args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amagg_internal::AmaggReqMeta;
  using amagg_internal::AmaggTypeWrapper;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  if (remote_proc == proc::rank_me()) {
    // local precedure call
    return amagg_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
  }

  Future<std::invoke_result_t<Fn, Args...>> future(true);
  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmaggTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmaggReqMeta meta{fn_p, wrapper_p, future.get_p(), remote_worker_local};
  Payload payload(std::forward<Args>(args)...);
  //  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
  //  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char *, int64_t> result = amagg_internal::amagg_agg_buffer_p[remote_proc]->push((char *) &meta, sizeof(meta), (char *) &payload, sizeof(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      backend::send_msg(remote_proc, am_internal::HandlerType::AM_REQ, std::get<0>(result), std::get<1>(result));
      progress_external();
    }
  }
  ++amagg_internal::amagg_req_local_counters[local::rank_me()].val;
  return future;
}

}// namespace arl

#endif//ARL_AM_AGG_HPP
