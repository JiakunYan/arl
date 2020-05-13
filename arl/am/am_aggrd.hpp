//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_AGGRD_HPP
#define ARL_AM_AGGRD_HPP

namespace arl {
namespace amaggrd_internal {

using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;
using am_internal::AggBuffer;
using am_internal::FutureData;

// GASNet AM handlers and their indexes
alignas(alignof_cacheline) gex_AM_Index_t hidx_gex_amaggrd_reqhandler;
alignas(alignof_cacheline) gex_AM_Index_t hidx_gex_amaggrd_ackhandler;
void gex_amaggrd_reqhandler(gex_Token_t token, void *buf, size_t nbytes,
                                gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3);
void gex_amaggrd_ackhandler(gex_Token_t token, void *buf, size_t nbytes,
                                gex_AM_Arg_t t0, gex_AM_Arg_t t1);
// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amaggrd_ack_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amaggrd_req_counter;
alignas(alignof_cacheline) AlignedInt64 *amaggrd_req_local_counters;

alignas(alignof_cacheline) AggBuffer* amaggrd_agg_buffer_p;

struct AmaggrdReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
};

template <typename... Args>
struct AmaggrdReqPayload {
  intptr_t future_p;
  int target_local_rank;
  std::tuple<Args...> data;
};

struct AmaggrdAckMeta {
  intptr_t type_wrapper_p;
};

template <typename T>
struct AmaggrdAckPayload {
  intptr_t future_p;
  T data;
};

template <>
struct AmaggrdAckPayload<void> {
  intptr_t future_p;
};

template <typename Fn, typename... Args>
class AmaggrdTypeWrapper {
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

  static int req_invoker(intptr_t fn_p, char* buf, int nbytes, char* output, int onbytes) {
    ARL_Assert(nbytes >= (int) sizeof(ReqPayload), "(", nbytes, " >= ", sizeof(ReqPayload), ")");
    ARL_Assert(nbytes % (int) sizeof(ReqPayload) == 0, "(", nbytes, " % ", sizeof(ReqPayload), " == 0)");
    ARL_Assert(onbytes >= nbytes / sizeof(ReqPayload) * sizeof(AckPayload), "(", onbytes, " >= ", nbytes, " / ", sizeof(AckPayload), " * ", my_sizeof<AckPayload>());

    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);

    for (int i = 0; i < nbytes / (int) sizeof(ReqPayload); ++i) {
      ReqPayload& payload = *reinterpret_cast<ReqPayload*>(buf + i * sizeof(ReqPayload));
      AckPayload& result = *reinterpret_cast<AckPayload*>(output + i * sizeof(AckPayload));
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

  static int ack_invoker(char* buf, int nbytes) {
    ARL_Assert(nbytes >= (int) sizeof(AckPayload), "(", nbytes, " >= ", sizeof(AckPayload), ")");
    ARL_Assert(nbytes % (int) sizeof(AckPayload) == 0, "(", nbytes, " % ", sizeof(AckPayload), " == 0)");

    for (int i = 0; i < nbytes / (int) sizeof(AckPayload); ++i) {
      AckPayload& result = *reinterpret_cast<AckPayload*>(buf + i * sizeof(AckPayload));
      auto* future_p = reinterpret_cast<FutureData<Result>*>(result.future_p);

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

  template <size_t... I>
  static Result run_fn(Fn* fn, const std::tuple<Args...>& data, std::index_sequence<I...>) {
    return std::invoke(*fn, std::get<I>(data)...);
  }
};

void gex_amaggrd_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes,
                            gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3) {
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t srcRank = info.gex_srcrank;
  gex_AM_Arg_t* arg_p = new gex_AM_Arg_t[4];
  arg_p[0] = t0; arg_p[1] = t1; arg_p[2] = t2; arg_p[3] = t3;
  char* buf_p = new char[unbytes];
  memcpy(buf_p, void_buf, unbytes);
  am_internal::UniformGexAMEventData event {
      am_internal::HandlerType::AM_RD_REQ, srcRank,
      4, arg_p,
      static_cast<int>(unbytes), buf_p
  };
  am_internal::am_event_queue_p->push(event);
}

void generic_amaggrd_reqhandler(const am_internal::UniformGexAMEventData& event) {
  using req_invoker_t = int(intptr_t, char*, int, char*, int);
  ARL_Assert(event.handler_type == am_internal::HandlerType::AM_RD_REQ);

  char* in_buf = event.buf_p;
  int in_nbytes = event.buf_n;
  ARL_Assert(in_nbytes >= (int) sizeof(AmaggrdReqMeta), "(", in_nbytes, " >= ", sizeof(AmaggrdReqMeta), ")");
  AmaggrdReqMeta& in_meta = *reinterpret_cast<AmaggrdReqMeta*>(event.arg_p);
//  printf("recv meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
  int out_nbytes = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,2);
  char* out_buf = new char[out_nbytes];

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(in_meta.type_wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
  int out_consumed = req_invoker(in_meta.fn_p, in_buf, in_nbytes, out_buf, out_nbytes);

  AmaggrdAckMeta out_meta{in_meta.type_wrapper_p};
  gex_AM_Arg_t* out_t = reinterpret_cast<gex_AM_Arg_t*>(&out_meta);
  gex_AM_RequestMedium2(backend::tm, event.srcRank, hidx_gex_amaggrd_ackhandler, out_buf, out_consumed, GEX_EVENT_NOW, 0,
                      out_t[0], out_t[1]);

  delete [] out_buf;
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void gex_amaggrd_ackhandler(gex_Token_t token, void *void_buf, size_t unbytes,
                                gex_AM_Arg_t t0, gex_AM_Arg_t t1) {
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t srcRank = info.gex_srcrank;
  gex_AM_Arg_t* arg_p = new gex_AM_Arg_t[2];
  arg_p[0] = t0; arg_p[1] = t1;
  char* buf_p = new char[unbytes];
  memcpy(buf_p, void_buf, unbytes);
  am_internal::UniformGexAMEventData event {
      am_internal::HandlerType::AM_RD_ACK, srcRank,
      2, arg_p,
      static_cast<int>(unbytes), buf_p
  };
  am_internal::am_event_queue_p->push(event);
}

void generic_amaggrd_ackhandler(const am_internal::UniformGexAMEventData& event) {
  using ack_invoker_t = int(char*, int);
  ARL_Assert(event.handler_type == am_internal::HandlerType::AM_RD_ACK);

  char* buf = event.buf_p;
  int nbytes = event.buf_n;
  ARL_Assert(nbytes >= (int) sizeof(AmaggrdAckMeta), "(", nbytes, " >= ", sizeof(AmaggrdAckMeta), ")");
  AmaggrdAckMeta& meta = *reinterpret_cast<AmaggrdAckMeta*>(event.arg_p);
//  printf("recv meta: %ld\n", meta.type_wrapper_p);

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(meta.type_wrapper_p);
  intptr_t ack_invoker_p = invoker("ack_invoker");
  auto ack_invoker = resolve_pi_fnptr<ack_invoker_t>(ack_invoker_p);
  int ack_n = ack_invoker(buf, nbytes);

  amaggrd_ack_counter->val += ack_n;
}

alignas(alignof_cacheline) AmaggrdReqMeta* global_meta_p = nullptr;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amaggrd() {
  hidx_gex_amaggrd_reqhandler = am_internal::gex_am_handler_num++;
  hidx_gex_amaggrd_ackhandler = am_internal::gex_am_handler_num++;
  ARL_Assert(am_internal::gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[2] = {
      { hidx_gex_amaggrd_reqhandler,
          (gex_AM_Fn_t) gex_amaggrd_reqhandler,
          GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
          4 },
      { hidx_gex_amaggrd_ackhandler,
          (gex_AM_Fn_t) gex_amaggrd_ackhandler,
          GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
          2 } };

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t));

  amaggrd_agg_buffer_p = new AggBuffer[proc::rank_n()];

  // TODO: might have problem if sizeof(result) > sizeof(arguments)
  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,4);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amaggrd_agg_buffer_p[i].init(max_buffer_size);
  }

  amaggrd_ack_counter = new AlignedAtomicInt64;
  amaggrd_ack_counter->val = 0;
  amaggrd_req_counter = new AlignedAtomicInt64;
  amaggrd_req_counter->val = 0;

  amaggrd_req_local_counters = new AlignedInt64[local::rank_n()];
  for (int i = 0; i < local::rank_n(); ++i) {
    amaggrd_req_local_counters[i].val = 0;
  }
}

void exit_amaggrd() {
  delete [] amaggrd_req_local_counters;
  delete global_meta_p;
  delete amaggrd_ack_counter;
  delete amaggrd_req_counter;
  delete [] amaggrd_agg_buffer_p;
}

void flush_amaggrd_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char*, int>> results = amaggrd_agg_buffer_p[i].flush();
    for (auto result: results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
//          printf("rank %ld send %p, %d\n", rank_me(), std::get<0>(result), std::get<1>(result));
          gex_AM_Arg_t* t = reinterpret_cast<gex_AM_Arg_t*>(amaggrd_internal::global_meta_p);
          gex_AM_RequestMedium4(backend::tm, i, amaggrd_internal::hidx_gex_amaggrd_reqhandler,
                                std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0,
                                t[0], t[1], t[2], t[3]);
        }
        delete [] std::get<0>(result);
      }
    }
  }
}

void wait_amaggrd() {
  amaggrd_req_counter->val += amaggrd_req_local_counters[local::rank_me()].val;
  amaggrd_req_local_counters[local::rank_me()].val = 0;
  local::barrier();
  while (amaggrd_req_counter->val > amaggrd_ack_counter->val) {
    progress();
  }
}
} // namespace amaggrd_internal

// TODO: handle function pointer situation
// TODO: adjust AggBuffer size
template <typename Fn, typename... Args>
void register_amaggrd(const Fn& fn) {
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
template <typename Fn, typename... Args>
void register_amaggrd(const Fn& fn, Args&&...) {
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

template <typename Fn, typename... Args>
Future<std::invoke_result_t<Fn, Args...>> rpc_aggrd(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = amaggrd_internal::AmaggrdReqPayload<std::remove_reference_t<Args>...>;
  using amaggrd_internal::AmaggrdTypeWrapper;
  using amaggrd_internal::AmaggrdReqMeta;

  ARL_Assert(amaggrd_internal::global_meta_p != nullptr, "call rpc_aggrd before register_aggrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
//  if (remote_proc == proc::rank_me()) {
//    // local precedure call
//    return amagg_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
//  }

  Future<std::invoke_result_t<Fn, Args...>> future;
  Payload payload{future.get_p(), remote_worker_local, std::make_tuple(std::forward<Args>(args)...)};
//  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
//  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char*, int> result = amaggrd_internal::amaggrd_agg_buffer_p[remote_proc].push(std::move(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      gex_AM_Arg_t* t = reinterpret_cast<gex_AM_Arg_t*>(amaggrd_internal::global_meta_p);
      gex_AM_RequestMedium4(backend::tm, remote_proc, amaggrd_internal::hidx_gex_amaggrd_reqhandler,
                            std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0,
                            t[0], t[1], t[2], t[3]);
    }
    delete [] std::get<0>(result);
  }
  ++(amaggrd_internal::amaggrd_req_local_counters[local::rank_me()].val);
  return future;
}

} // namespace arl

#endif //ARL_AM_AGGRD_HPP
