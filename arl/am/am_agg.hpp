//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_AGG_HPP
#define ARL_AM_AGG_HPP

namespace arl {
namespace amagg_internal {

using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;
using am_internal::AggBuffer;
using am_internal::FutureData;

// GASNet AM handlers and their indexes
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_amagg_reqhandler;
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_amagg_ackhandler;
void generic_amagg_reqhandler(gex_Token_t token, void *buf, size_t nbytes);
void generic_amagg_ackhandler(gex_Token_t token, void *buf, size_t nbytes);// AM synchronous counters
// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amagg_ack_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amagg_req_counter;
alignas(alignof_cacheline) AlignedInt64 *amagg_req_local_counters;

alignas(alignof_cacheline) AggBuffer* amagg_agg_buffer_p;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amagg() {
  hidx_generic_amagg_reqhandler = am_internal::gex_am_handler_num++;
  hidx_generic_amagg_ackhandler = am_internal::gex_am_handler_num++;
  ARL_Assert(am_internal::gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[2] = {
      { hidx_generic_amagg_reqhandler,
        (gex_AM_Fn_t) generic_amagg_reqhandler,
        GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
        0 },
      { hidx_generic_amagg_ackhandler,
        (gex_AM_Fn_t) generic_amagg_ackhandler,
        GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REPLY,
        0 } };

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t));

  amagg_agg_buffer_p = new AggBuffer[proc::rank_n()];

  // TODO: might have problem if sizeof(result) > sizeof(arguments)
  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amagg_agg_buffer_p[i].init(max_buffer_size);
  }
  amagg_ack_counter = new AlignedAtomicInt64;
  amagg_ack_counter->val = 0;
  amagg_req_counter = new AlignedAtomicInt64;
  amagg_req_counter->val = 0;

  amagg_req_local_counters = new AlignedInt64[local::rank_n()];
  for (int i = 0; i < local::rank_n(); ++i) {
    amagg_req_local_counters[i].val = 0;
  }
}

void exit_amagg() {
  delete [] amagg_req_local_counters;
  delete amagg_req_counter;
  delete amagg_ack_counter;
  delete [] amagg_agg_buffer_p;
}

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

template <typename Fn, typename... Args>
class AmaggTypeWrapper {
 public:
  static intptr_t invoker(const std::string& cmd) {
    if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else if (cmd == "ack_invoker") {
      return get_pi_fnptr(ack_invoker);
    } else {
      throw std::runtime_error("Unknown function call");
    }
  }

  static std::pair<int, int> req_invoker(intptr_t fn_p, int context, char* buf, int nbytes, char* output, int onbytes) {
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    if constexpr (! std::is_void_v<Result>)
      ARL_Assert(onbytes >= (int) my_sizeof<Result>(), "(", onbytes, " >= ", my_sizeof<Result>(), ")");

    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);
    auto* ptr = reinterpret_cast<Payload*>(buf);

    rank_t mContext = get_context();
    set_context(context);
    if constexpr (! std::is_void_v<Result>)
      *reinterpret_cast<Result*>(output) = run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    else
      run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    set_context(mContext);
    return std::make_pair(sizeof(Payload), my_sizeof<Result>());
  }

  static int ack_invoker(intptr_t raw_future_p, char* buf, int nbytes) {
    ARL_Assert(nbytes >= (int) my_sizeof<Result>(), "(", nbytes, " >= ", my_sizeof<Result>(), ")");
    auto* future_p = reinterpret_cast<FutureData<Result>*>(raw_future_p);

    if constexpr (!std::is_void_v<Result>) {
      auto* ptr = reinterpret_cast<Result*>(buf);
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

  template <size_t... I>
  static Result run_fn(Fn* fn, const Payload& data, std::index_sequence<I...>) {
    return std::invoke(*fn, std::get<I>(data)...);
  }
};

void generic_amagg_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes) {
  using req_invoker_t = std::pair<int, int>(intptr_t, int, char*, int, char*, int);
//  printf("rank %ld amagg reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = static_cast<char*>(void_buf);
  int nbytes = static_cast<int>(unbytes);
  int o_cap = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  char* o_buf = new char[o_cap];
  int o_consumed = 0;
  int n = 0;
  int consumed = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmaggReqMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmaggReqMeta), ")");
    AmaggReqMeta& meta = *reinterpret_cast<AmaggReqMeta*>(buf + consumed);
//    printf("meta: %ld, %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.future_p, meta.target_local_rank);
    consumed += sizeof(AmaggReqMeta);

    ARL_Assert(o_cap >= o_consumed + (int) sizeof(AmaggAckMeta), "(", o_cap, " >= ", o_consumed, " + ", sizeof(AmaggReqMeta), ")");
    AmaggAckMeta& ackMeta = *reinterpret_cast<AmaggAckMeta*>(o_buf + o_consumed);
    ackMeta.type_wrapper_p = meta.type_wrapper_p;
    ackMeta.future_p = meta.future_p;
    o_consumed += sizeof(AmaggAckMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(meta.type_wrapper_p);
    intptr_t req_invoker_p = invoker("req_invoker");
    auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
//    printf("o_consumed is %d\n", o_consumed);
    std::pair<int, int> consumed_data = req_invoker(meta.fn_p, meta.target_local_rank, buf + consumed, nbytes - consumed, o_buf + o_consumed, o_cap - o_consumed);
    consumed += std::get<0>(consumed_data);
    o_consumed += std::get<1>(consumed_data);
    ++n;
  }
  gex_AM_ReplyMedium0(token, hidx_generic_amagg_ackhandler, o_buf, o_consumed, GEX_EVENT_NOW, 0);
  delete [] o_buf;
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void generic_amagg_ackhandler(gex_Token_t token, void *void_buf, size_t unbytes) {
  using ack_invoker_t = int(intptr_t, char*, int);
//  printf("rank %ld amagg ackhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = static_cast<char*>(void_buf);
  int nbytes = static_cast<int>(unbytes);
  int consumed = 0;
  int n = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmaggAckMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmaggAckMeta), ")");
    AmaggAckMeta& meta = *reinterpret_cast<AmaggAckMeta*>(buf + consumed);
//    printf("meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
    consumed += sizeof(AmaggAckMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(meta.type_wrapper_p);
    intptr_t ack_invoker_p = invoker("ack_invoker");
    auto ack_invoker = resolve_pi_fnptr<ack_invoker_t>(ack_invoker_p);
    consumed += ack_invoker(meta.future_p, buf + consumed, nbytes - consumed);
    ++n;
  }
  amagg_internal::amagg_ack_counter->val += n;
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void flush_amagg_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char*, int>> results = amagg_agg_buffer_p[i].flush();
    for (auto result: results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
//          printf("rank %ld send %p, %d\n", rank_me(), std::get<0>(result), std::get<1>(result));
          gex_AM_RequestMedium0(backend::tm, i, amagg_internal::hidx_generic_amagg_reqhandler,
                                std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0);
        }
        delete [] std::get<0>(result);
      }
    }
  }
}

void wait_amagg() {
  amagg_req_counter->val += amagg_req_local_counters[local::rank_me()].val;
  amagg_req_local_counters[local::rank_me()].val = 0;
  local::barrier();
  while (amagg_req_counter->val > amagg_ack_counter->val) {
    progress();
  }
}
} // namespace amagg_internal

template <typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amagg_internal::AmaggTypeWrapper;
  using amagg_internal::AmaggReqMeta;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();

  Future<std::invoke_result_t<Fn, Args...>> future;
  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmaggTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmaggReqMeta meta{fn_p, wrapper_p, future.get_p(), remote_worker_local};
  Payload payload(std::forward<Args>(args)...);
//  printf("send meta: %ld, %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.future_p, meta.target_local_rank);
//  printf("sizeof(payload): %lu\n", sizeof(Payload));

  char* ptr = new char[sizeof(AmaggReqMeta) + sizeof(Payload)];
  *reinterpret_cast<AmaggReqMeta*>(ptr) = meta;
  *reinterpret_cast<Payload*>(ptr+sizeof(AmaggReqMeta)) = payload;
  gex_AM_RequestMedium0(backend::tm, remote_proc, amagg_internal::hidx_generic_amagg_reqhandler,
                        ptr, sizeof(AmaggReqMeta) + sizeof(Payload), GEX_EVENT_NOW, 0);
  delete [] ptr;
  ++amagg_internal::amagg_req_local_counters[local::rank_me()].val;
  return future;
}

template <typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc_agg(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amagg_internal::AmaggTypeWrapper;
  using amagg_internal::AmaggReqMeta;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();

  Future<std::invoke_result_t<Fn, Args...>> future;
  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmaggTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmaggReqMeta meta{fn_p, wrapper_p, future.get_p(), remote_worker_local};
  Payload payload(std::forward<Args>(args)...);
//  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
//  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char*, int> result = amagg_internal::amagg_agg_buffer_p[remote_proc].push(meta, std::move(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      gex_AM_RequestMedium0(backend::tm, remote_proc, amagg_internal::hidx_generic_amagg_reqhandler,
                            std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0);
    }
    delete [] std::get<0>(result);
  }
  ++amagg_internal::amagg_req_local_counters[local::rank_me()].val;
  return future;
}

} // namespace arl

#endif //ARL_AM_AGG_HPP
