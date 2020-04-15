//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_FF_HPP
#define ARL_AM_FF_HPP

namespace arl {
namespace amff_internal {

using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;
using am_internal::AggBuffer;

// GASNet AM handlers and their indexes
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_am_ff_reqhandler;
void generic_am_ff_reqhandler(gex_Token_t token, void *buf, size_t nbytes);
// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_recv_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_req_counters; // of length proc::rank_n()

alignas(alignof_cacheline) AggBuffer* amff_agg_buffer_p;


// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_am_ff() {
  hidx_generic_am_ff_reqhandler = am_internal::gex_am_handler_num++;
  ARL_Assert(am_internal::gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[1] = {
      { hidx_generic_am_ff_reqhandler,
        (gex_AM_Fn_t) generic_am_ff_reqhandler,
        GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
        0 }
  };

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t));

  amff_agg_buffer_p = new AggBuffer[proc::rank_n()];

  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_agg_buffer_p[i].init(max_buffer_size);
  }
  amff_recv_counter = new AlignedAtomicInt64;
  amff_recv_counter->val = 0;
  amff_req_counters = new AlignedAtomicInt64[proc::rank_n()];
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_req_counters[i].val = 0;
  }
}

void exit_am_ff() {
  delete [] amff_req_counters;
  delete amff_recv_counter;
  delete [] amff_agg_buffer_p;
}

struct AmffReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
  int target_local_rank;
};

template <typename Fn, typename... Args>
class AmffTypeWrapper {
 public:
  static intptr_t invoker(const std::string& cmd) {
    if (cmd == "payload_size") {
      return get_pi_fnptr(payload_size);
    } else if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else {
      throw std::runtime_error("Unknown function call");
    }
  }

  static constexpr int payload_size() {
    return sizeof(Payload);
  }

  static void req_invoker(intptr_t fn_p, int context, char* buf, int nbytes) {
    static_assert( std::is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);
    Payload *ptr = reinterpret_cast<Payload*>(buf);

    rank_t mContext = get_context();
    set_context(context);
    run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    set_context(mContext);
  }

 private:
  using Payload = std::tuple<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template <size_t... I>
  static void run_fn(Fn* fn, const Payload& data, std::index_sequence<I...>) {
    std::invoke(*fn,  std::get<I>(data)...);
  }
};

void generic_am_ff_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes) {
  using payload_size_t = int();
  using req_invoker_t = void(intptr_t, int, char*, int);
//  printf("rank %ld amff reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = static_cast<char*>(void_buf);
  int nbytes = static_cast<int>(unbytes);
  int n = 0;
  int consumed = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmffReqMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmffReqMeta), ")");
    AmffReqMeta& meta = *reinterpret_cast<AmffReqMeta*>(buf + consumed);
//    printf("meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
    consumed += sizeof(AmffReqMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(meta.type_wrapper_p);

    intptr_t payload_size_p = invoker("payload_size");
    auto payload_size = resolve_pi_fnptr<payload_size_t>(payload_size_p);
    int payload_nbytes = payload_size();
//    printf("payload_nbytes: %d\n", payload_nbytes);

    intptr_t req_invoker_p = invoker("req_invoker");
    auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
    req_invoker(meta.fn_p, meta.target_local_rank, buf + consumed, nbytes - consumed);
    consumed += payload_nbytes;
    ++n;
  }

//  printf("rank %ld increase amff recv counter %ld by %d\n", rank_me(), amff_recv_counter->val.load(), n);
  amff_recv_counter->val += n;
//  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void flush_am_ff_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char*, int>> results = amff_agg_buffer_p[i].flush();
    for (auto result: results) {
      if ( std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          gex_AM_RequestMedium0(backend::tm, i, hidx_generic_am_ff_reqhandler,
                                std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0);
        }
        delete [] std::get<0>(result);
      }
    }
  }
}

int64_t get_expected_recv_num() {
  int64_t expected_recv_num = 0;
  if (local::rank_me() == 0) {
    for (int i = 0; i < proc::rank_n(); ++i) {
      if (i == proc::rank_me()) {
        expected_recv_num = proc::reduce_one(amff_req_counters[i].val.load(), op_plus(), i);
      } else {
        proc::reduce_one(amff_req_counters[i].val.load(), op_plus(), i);
      }
    }
  }
  return local::broadcast(expected_recv_num, 0);
}

void wait_amff() {
  local::barrier();
  int expected_recv_num = get_expected_recv_num();
  while (expected_recv_num > amff_recv_counter->val) {
    progress();
  }
}
} // namespace amff_internal

template <typename Fn, typename... Args>
void rpc_ff(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = std::tuple<std::remove_reference_t<Args>...>;
  using amff_internal::AmffTypeWrapper;
  using amff_internal::AmffReqMeta;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();

  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmffTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker);
  AmffReqMeta meta{fn_p, wrapper_p, remote_worker_local};
  Payload payload(std::forward<Args>(args)...);

//  std::pair<am_internal::AmffReqMeta, Payload> data{meta, payload};
//  printf("send meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
//  printf("sizeof(payload): %lu\n", sizeof(Payload));

  std::pair<char*, int> result = amff_internal::amff_agg_buffer_p[remote_proc].push(meta, std::move(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      gex_AM_RequestMedium0(backend::tm, remote_proc, amff_internal::hidx_generic_am_ff_reqhandler,
                            std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0);
    }
    delete [] std::get<0>(result);
  }
  ++(amff_internal::amff_req_counters[remote_proc].val);
}

} // namespace arl

#endif //ARL_AM_FF_HPP
