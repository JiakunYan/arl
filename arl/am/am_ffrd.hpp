//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_FFRD_HPP
#define ARL_AM_FFRD_HPP

namespace arl {
namespace amffrd_internal {

using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;
using am_internal::AggBuffer;

// GASNet AM handlers and their indexes
alignas(alignof_cacheline) gex_AM_Index_t hidx_gex_amffrd_reqhandler;
void gex_amffrd_reqhandler(gex_Token_t token, void *buf, size_t nbytes,
                               gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3);
// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amffrd_recv_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amffrd_req_counters; // of length proc::rank_n()
// other variables whose names are clear
alignas(alignof_cacheline) AggBuffer* amffrd_agg_buffer_p;

struct AmffrdReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
};

template <typename... Args>
struct AmffrdReqPayload {
  int target_local_rank;
  std::tuple<Args...> data;
};

template <typename Fn, typename... Args>
class AmffrdTypeWrapper {
 public:
  static intptr_t invoker(const std::string& cmd) {
    if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else {
      throw std::runtime_error("Unknown function call");
    }
  }

  static int req_invoker(intptr_t fn_p, char* buf, int nbytes) {
    static_assert( std::is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    ARL_Assert(nbytes % (int) sizeof(Payload) == 0, "(", nbytes, " % ", sizeof(Payload), " == 0)");
//    printf("req_invoker: fn_p %ld, buf %p, nbytes %d, sizeof(Payload) %lu\n", fn_p, buf, nbytes, sizeof(Payload));
    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);
    auto ptr = reinterpret_cast<Payload*>(buf);

    for (int i = 0; i < nbytes; i += sizeof(Payload)) {
      Payload& payload = *reinterpret_cast<Payload*>(buf + i);
      rank_t mContext = rank_internal::get_context();
      rank_internal::set_context(payload.target_local_rank);
      run_fn(fn, payload.data, std::index_sequence_for<Args...>());
      rank_internal::set_context(mContext);
    }
    return nbytes / (int) sizeof(Payload);
  }

 private:
  using Payload = AmffrdReqPayload<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template <size_t... I>
  static void run_fn(Fn* fn, const std::tuple<Args...>& data, std::index_sequence<I...>) {
    std::invoke(*fn, std::get<I>(data)...);
  }
};

void gex_amffrd_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes,
                               gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3) {
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t srcRank = info.gex_srcrank;
  gex_AM_Arg_t* arg_p = new gex_AM_Arg_t[4];
  arg_p[0] = t0; arg_p[1] = t1; arg_p[2] = t2; arg_p[3] = t3;
  char* buf_p = new char[unbytes];
  memcpy(buf_p, void_buf, unbytes);
  am_internal::UniformGexAMEventData event {
      am_internal::HandlerType::AM_FFRD_REQ, srcRank,
      4, arg_p,
      static_cast<int>(unbytes), buf_p
  };
  am_internal::am_event_queue_p->push(event);
  info::networkInfo.byte_recv.add(unbytes);
}

void generic_amffrd_reqhandler(const am_internal::UniformGexAMEventData& event) {
  using req_invoker_t = int(intptr_t, char*, int);
//  printf("rank %ld reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = event.buf_p;
  int nbytes = event.buf_n;
  ARL_Assert(nbytes >= (int) sizeof(AmffrdReqMeta), "(", nbytes, " >= ", sizeof(AmffrdReqMeta), ")");
  AmffrdReqMeta& meta = *reinterpret_cast<AmffrdReqMeta*>(event.arg_p);
//  printf("recv meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(meta.type_wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
  int ack_n = req_invoker(meta.fn_p, buf, nbytes);

  amffrd_recv_counter->val += ack_n;
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void send_amffrd_to_gex(rank_t remote_proc, AmffrdReqMeta meta, void* buf, size_t nbytes) {
  gex_AM_Arg_t* ptr = reinterpret_cast<gex_AM_Arg_t*>(&meta);
//  printf("send meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
  gex_AM_RequestMedium4(backend::tm, remote_proc, amffrd_internal::hidx_gex_amffrd_reqhandler,
                        buf, nbytes, GEX_EVENT_NOW, 0, ptr[0], ptr[1], ptr[2], ptr[3]);
  info::networkInfo.byte_send.add(nbytes);
}

alignas(alignof_cacheline) AmffrdReqMeta* global_meta_p = nullptr;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amffrd() {
  amffrd_internal::hidx_gex_amffrd_reqhandler = am_internal::gex_am_handler_num++;
  ARL_Assert(am_internal::gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[1] = {
      {amffrd_internal::hidx_gex_amffrd_reqhandler,
          (gex_AM_Fn_t) amffrd_internal::gex_amffrd_reqhandler,
          GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
          4},
  };

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable) / sizeof(gex_AM_Entry_t));

  amffrd_internal::amffrd_agg_buffer_p = new amffrd_internal::AggBuffer[proc::rank_n()];

  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm, GEX_RANK_INVALID, GEX_EVENT_NOW, 0, 4);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amffrd_internal::amffrd_agg_buffer_p[i].init(max_buffer_size);
  }

  amffrd_recv_counter = new AlignedAtomicInt64;
  amffrd_recv_counter->val = 0;
  amffrd_req_counters = new AlignedAtomicInt64[proc::rank_n()];
  for (int i = 0; i < proc::rank_n(); ++i) {
    amffrd_req_counters[i].val = 0;
  }
}

void exit_amffrd() {
  delete [] amffrd_req_counters;
  delete amffrd_recv_counter;
  delete amffrd_internal::global_meta_p;
  delete[] amffrd_internal::amffrd_agg_buffer_p;
}

void flush_amffrd_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char*, int>> results = amffrd_internal::amffrd_agg_buffer_p[i].flush();
    for (auto result: results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          amffrd_internal::send_amffrd_to_gex(i, *amffrd_internal::global_meta_p,
                                              std::get<0>(result), std::get<1>(result));
          progress_external();
        }
        delete [] std::get<0>(result);
      }
    }
  }
}

int64_t get_expected_recv_num() {
  int64_t expected_recv_num = 0;
  if (local::rank_me() == 0) {
    std::vector<int64_t> src_v(proc::rank_n());
    for (int i = 0; i < proc::rank_n(); ++i) {
      src_v[i] = amffrd_req_counters[i].val.load();
    }
    auto dst_v = proc::reduce_all(src_v, op_plus());
    expected_recv_num = dst_v[proc::rank_me()];
  }
  return local::broadcast(expected_recv_num, 0);
}

void wait_amffrd() {
  local::barrier();
  int64_t expected_recv_num = get_expected_recv_num();
  while (expected_recv_num > amffrd_internal::amffrd_recv_counter->val) {
    progress();
  }
}

} // namespace amffrd_internal

// TODO: handle function pointer situation
template <typename Fn, typename... Args>
void register_amffrd(const Fn& fn) {
  pure_barrier();
  if (local::rank_me() == 0) {
    delete amffrd_internal::global_meta_p;
    intptr_t fn_p = amffrd_internal::get_pi_fnptr(&fn);
    intptr_t wrapper_p = amffrd_internal::get_pi_fnptr(&amffrd_internal::AmffrdTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
    amffrd_internal::global_meta_p = new amffrd_internal::AmffrdReqMeta{fn_p, wrapper_p};
//    printf("register meta: %ld, %ld\n", amffrd_internal::global_meta_p->fn_p, amffrd_internal::global_meta_p->type_wrapper_p);
  }
  pure_barrier();
}

// TODO: handle function pointer situation
template <typename Fn, typename... Args>
void register_amffrd(Fn&& fn, Args&&...) {
  pure_barrier();
  if (local::rank_me() == 0) {
    delete amffrd_internal::global_meta_p;
    intptr_t fn_p = amffrd_internal::get_pi_fnptr(&fn);
    intptr_t wrapper_p = amffrd_internal::get_pi_fnptr(&amffrd_internal::AmffrdTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
    amffrd_internal::global_meta_p = new amffrd_internal::AmffrdReqMeta{fn_p, wrapper_p};
//    printf("register meta: %ld, %ld\n", amffrd_internal::global_meta_p->fn_p, amffrd_internal::global_meta_p->type_wrapper_p);
  }
  pure_barrier();
}

// TODO: memcpy on tuple is dangerous
template <typename Fn, typename... Args>
void rpc_ffrd(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = amffrd_internal::AmffrdReqPayload<std::remove_reference_t<Args>...>;
  using amffrd_internal::AmffrdTypeWrapper;
  using amffrd_internal::AmffrdReqMeta;

  ARL_Assert(amffrd_internal::global_meta_p != nullptr, "call rpc_ffrd before register_ffrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
//  if (remote_proc == proc::rank_me()) {
//    // local precedure call
//    Fn* fn_p = am_internal::resolve_pi_fnptr<Fn>(amffrd_internal::global_meta_p->fn_p);
//    amff_internal::run_lpc(remote_worker_local, *fn_p, std::forward<Args>(args)...);
//    return;
//  }

  Payload payload{remote_worker_local, std::make_tuple(std::forward<Args>(args)...)};
//  printf("Rank %ld send rpc to rank %ld\n", rank_me(), remote_worker);
//  std::cout << "sizeof(" << type_name<Payload>() << ") is " << sizeof(Payload) << std::endl;

  std::pair<char*, int> result = amffrd_internal::amffrd_agg_buffer_p[remote_proc].push(std::move(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      amffrd_internal::send_amffrd_to_gex(remote_proc, *amffrd_internal::global_meta_p,
                                          std::get<0>(result), std::get<1>(result));
      progress_external();
    }
    delete [] std::get<0>(result);
  }
  ++(amffrd_internal::amffrd_req_counters[remote_proc].val);
}

} // namespace arl

#endif //ARL_AM_FFRD_HPP
