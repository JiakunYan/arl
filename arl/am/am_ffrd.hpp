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
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_amffrd_reqhandler;
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_amffrd_ackhandler;
void generic_amffrd_reqhandler(gex_Token_t token, void *buf, size_t nbytes,
                               gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3);
void generic_amffrd_ackhandler(gex_Token_t token, gex_AM_Arg_t n);
// AM synchronous counters
alignas(alignof_cacheline) std::atomic<int64_t> *amffrd_ack_counter;
alignas(alignof_cacheline) std::atomic<int64_t> *amffrd_req_counter;
// other variables whose names are clear
AggBuffer* amffrd_agg_buffer_p;

struct AmffrdReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
};

template <typename... Args>
struct AmffrdReqPayload {
  int target_local_rank;
  tuple<Args...> data;
};

template <typename Fn, typename... Args>
class AmffrdTypeWrapper {
 public:
  static intptr_t invoker(const std::string& cmd) {
    if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else {
      throw runtime_error("Unknown function call");
    }
  }

  static int req_invoker(intptr_t fn_p, char* buf, int nbytes) {
    static_assert(is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    ARL_Assert(nbytes % (int) sizeof(Payload) == 0, "(", nbytes, " % ", sizeof(Payload), " == 0)");
//    printf("req_invoker: fn_p %ld, buf %p, nbytes %d, sizeof(Payload) %lu\n", fn_p, buf, nbytes, sizeof(Payload));
    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);
    auto ptr = reinterpret_cast<Payload*>(buf);

    for (int i = 0; i < nbytes; i += sizeof(Payload)) {
      Payload& payload = *reinterpret_cast<Payload*>(buf + i);
      rank_t mContext = get_context();
      set_context(payload.target_local_rank);
      run_fn(fn, payload.data, index_sequence_for<Args...>());
      set_context(mContext);
    }
    return nbytes / (int) sizeof(Payload);
  }

 private:
  using Payload = AmffrdReqPayload<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template <size_t... I>
  static void run_fn(Fn* fn, const tuple<Args...>& data, index_sequence<I...>) {
    invoke(*fn, get<I>(data)...);
  }
};

void generic_amffrd_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes,
                               gex_AM_Arg_t t0, gex_AM_Arg_t t1, gex_AM_Arg_t t2, gex_AM_Arg_t t3) {
  using req_invoker_t = int(intptr_t, char*, int);
//  printf("rank %ld reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = static_cast<char*>(void_buf);
  int nbytes = static_cast<int>(unbytes);
  ARL_Assert(nbytes >= (int) sizeof(AmffrdReqMeta), "(", nbytes, " >= ", sizeof(AmffrdReqMeta), ")");
  AmffrdReqMeta meta;
  gex_AM_Arg_t* t = reinterpret_cast<gex_AM_Arg_t*>(&meta);
  t[0] = t0; t[1] = t1; t[2] = t2; t[3] = t3;
//  printf("recv meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);

  auto invoker = resolve_pi_fnptr<intptr_t(const string&)>(meta.type_wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
  int ack_n = req_invoker(meta.fn_p, buf, nbytes);
  gex_AM_ReplyShort1(token, hidx_generic_amffrd_ackhandler, 0, static_cast<gex_AM_Arg_t>(ack_n));
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void generic_amffrd_ackhandler(gex_Token_t token, gex_AM_Arg_t n) {
  *amffrd_ack_counter += static_cast<int>(n);
//  printf("rank %ld ackhandler %d, %ld\n", rank_me(), n, amffrd_ack_counter->load());
}

void send_amffrd_to_gex(rank_t remote_proc, AmffrdReqMeta meta, void* buf, size_t nbytes) {
  gex_AM_Arg_t* ptr = reinterpret_cast<gex_AM_Arg_t*>(&meta);
//  printf("send meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
  gex_AM_RequestMedium4(backend::tm, remote_proc, amffrd_internal::hidx_generic_amffrd_reqhandler,
                        buf, nbytes, GEX_EVENT_NOW, 0, ptr[0], ptr[1], ptr[2], ptr[3]);
}

AmffrdReqMeta* global_meta_p = nullptr;

// Currently, init_am_ff should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amffrd() {
  amffrd_internal::hidx_generic_amffrd_reqhandler = am_internal::gex_am_handler_num++;
  amffrd_internal::hidx_generic_amffrd_ackhandler = am_internal::gex_am_handler_num++;
  ARL_Assert(am_internal::gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[2] = {
      {amffrd_internal::hidx_generic_amffrd_reqhandler,
          (gex_AM_Fn_t) amffrd_internal::generic_amffrd_reqhandler,
          GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
          4},
      {amffrd_internal::hidx_generic_amffrd_ackhandler,
          (gex_AM_Fn_t) amffrd_internal::generic_amffrd_ackhandler,
          GEX_FLAG_AM_SHORT | GEX_FLAG_AM_REPLY,
          1}};

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable) / sizeof(gex_AM_Entry_t));

  amffrd_internal::amffrd_agg_buffer_p = new amffrd_internal::AggBuffer[proc::rank_n()];

  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm, GEX_RANK_INVALID, GEX_EVENT_NOW, 0, 4);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amffrd_internal::amffrd_agg_buffer_p[i].init(max_buffer_size);
  }

  amffrd_internal::amffrd_ack_counter = new std::atomic<int64_t>;
  *amffrd_internal::amffrd_ack_counter = 0;
  amffrd_internal::amffrd_req_counter = new std::atomic<int64_t>;
  *amffrd_internal::amffrd_req_counter = 0;
}

void exit_amffrd() {
  delete amffrd_internal::global_meta_p;
  delete[] amffrd_internal::amffrd_agg_buffer_p;
  delete amffrd_internal::amffrd_req_counter;
  delete amffrd_internal::amffrd_ack_counter;
}

void flush_amffrd() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    vector<pair<char*, int>> results = amffrd_internal::amffrd_agg_buffer_p[i].flush();
    for (auto result: results) {
      if (get<0>(result) != nullptr) {
        if (get<1>(result) != 0) {
          amffrd_internal::send_amffrd_to_gex(i, *amffrd_internal::global_meta_p,
                                              get<0>(result), get<1>(result));
        }
        delete [] get<0>(result);
      }
    }
  }
}

void wait_amffrd() {
  while (*amffrd_internal::amffrd_req_counter > *amffrd_internal::amffrd_ack_counter) {
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
    intptr_t wrapper_p = amffrd_internal::get_pi_fnptr(&amffrd_internal::AmffrdTypeWrapper<remove_reference_t<Fn>, remove_reference_t<Args>...>::invoker);
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
    intptr_t wrapper_p = amffrd_internal::get_pi_fnptr(&amffrd_internal::AmffrdTypeWrapper<remove_reference_t<Fn>, remove_reference_t<Args>...>::invoker);
    amffrd_internal::global_meta_p = new amffrd_internal::AmffrdReqMeta{fn_p, wrapper_p};
//    printf("register meta: %ld, %ld\n", amffrd_internal::global_meta_p->fn_p, amffrd_internal::global_meta_p->type_wrapper_p);
  }
  pure_barrier();
}

// TODO: memcpy on tuple is dangerous
template <typename... Args>
void rpc_ffrd(rank_t remote_worker, Args&&... args) {
  using Payload = amffrd_internal::AmffrdReqPayload<remove_reference_t<Args>...>;
  using amffrd_internal::AmffrdTypeWrapper;
  using amffrd_internal::AmffrdReqMeta;

  ARL_Assert(amffrd_internal::global_meta_p != nullptr, "call rpc_ffrd before register_ffrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  Payload payload{remote_worker_local, make_tuple(forward<Args>(args)...)};
//  printf("Rank %ld send rpc to rank %ld\n", rank_me(), remote_worker);
//  std::cout << "sizeof(" << type_name<Payload>() << ") is " << sizeof(Payload) << std::endl;

  pair<char*, int> result = amffrd_internal::amffrd_agg_buffer_p[remote_proc].push(std::move(payload));
  if (get<0>(result) != nullptr) {
    if (get<1>(result) != 0) {
      amffrd_internal::send_amffrd_to_gex(remote_proc, *amffrd_internal::global_meta_p,
                                          get<0>(result), get<1>(result));
    }
    delete [] get<0>(result);
  }
  ++(*amffrd_internal::amffrd_req_counter);
}

} // namespace arl

#endif //ARL_AM_FFRD_HPP
