//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_FF_HPP
#define ARL_AM_FF_HPP

#include <atomic>
#include "arl/global_decl.hpp"
#include "arl/base/rank.hpp"
#include <gasnetex.h>

namespace arl {
namespace am_internal {

// GASNet AM handlers and their indexes
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_am_ff_reqhandler;
alignas(alignof_cacheline) gex_AM_Index_t hidx_generic_am_ff_ackhandler;
void generic_am_ff_reqhandler(gex_Token_t token, void *buf, size_t nbytes);
void generic_am_ff_ackhandler(gex_Token_t token, gex_AM_Arg_t n);

AggBuffer* amff_agg_buffer_p;

// Currently, init_am_ff should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_am_ff() {
  gex_am_handler_num = GEX_AM_INDEX_BASE;

  hidx_generic_am_ff_reqhandler = gex_am_handler_num++;
  hidx_generic_am_ff_ackhandler = gex_am_handler_num++;
  ARL_Assert(gex_am_handler_num < 256, "GASNet handler index overflow!");

  gex_AM_Entry_t htable[2] = {
      { hidx_generic_am_ff_reqhandler,
        (gex_AM_Fn_t) generic_am_ff_reqhandler,
        GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
        0 },
      { hidx_generic_am_ff_ackhandler,
        (gex_AM_Fn_t) generic_am_ff_ackhandler,
        GEX_FLAG_AM_SHORT | GEX_FLAG_AM_REPLY,
        1 } };

  gex_EP_RegisterHandlers(backend::ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t));

  amff_agg_buffer_p = new AggBuffer[proc::rank_n()];

  int max_buffer_size = gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_agg_buffer_p[i].init(max_buffer_size);
  }
}

void exit_am_ff() {
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
      throw runtime_error("Unknown function call");
    }
  }

  static constexpr int payload_size() {
    return sizeof(Payload);
  }

  static void req_invoker(intptr_t fn_p, int context, char* buf, int nbytes) {
    static_assert(is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    Fn* fn = resolve_pi_fnptr<Fn>(fn_p);
    Payload *ptr = reinterpret_cast<Payload*>(buf);

    rank_t mContext = get_context();
    set_context(context);
    run_fn(fn, *ptr, index_sequence_for<Args...>());
    set_context(mContext);
  }

 private:
  using Payload = std::tuple<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template <size_t... I>
  static void run_fn(Fn* fn, const Payload& data, index_sequence<I...>) {
    invoke(*fn, get<I>(data)...);
  }
};

void generic_am_ff_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes) {
  using payload_size_t = int();
  using req_invoker_t = void(intptr_t, int, char*, int);
//  printf("reqhandler %p, %lu\n", void_buf, unbytes);

  char* buf = static_cast<char*>(void_buf);
  int nbytes = static_cast<int>(unbytes);
  int n = 0;
  int consumed = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmffReqMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmffReqMeta), ")");
    AmffReqMeta& meta = *reinterpret_cast<AmffReqMeta*>(buf + consumed);
//    printf("meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
    consumed += sizeof(AmffReqMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const string&)>(meta.type_wrapper_p);

    intptr_t payload_size_p = invoker("payload_size");
    auto payload_size = resolve_pi_fnptr<payload_size_t>(payload_size_p);
    int payload_nbytes = payload_size();

    intptr_t req_invoker_p = invoker("req_invoker");
    auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
    req_invoker(meta.fn_p, meta.target_local_rank, buf + consumed, nbytes - consumed);
    consumed += payload_nbytes;
    ++n;
  }
  gex_AM_ReplyShort1(token, hidx_generic_am_ff_ackhandler, 0, static_cast<gex_AM_Arg_t>(n));
}

void generic_am_ff_ackhandler(gex_Token_t token, gex_AM_Arg_t n) {
  *am_ack_counter += static_cast<int>(n);
}

void flush_am_ff_buffer() {
  if constexpr (is_same_v<AggBuffer, AggBufferLocal>) {
    for (int ii = 0; ii < proc::rank_n(); ++ii) {
      int i = (ii + local::rank_me()) % proc::rank_n();
      vector<pair<char*, int>> results = amff_agg_buffer_p[i].flush();
      for (auto result: results) {
        if (get<0>(result) != nullptr) {
          if (get<1>(result) != 0) {
            gex_AM_RequestMedium0(backend::tm, i, am_internal::hidx_generic_am_ff_reqhandler,
                                  get<0>(result), get<1>(result), GEX_EVENT_NOW, 0);
          }
          delete [] get<0>(result);
        }
      }
    }
  } else {
    for (int i = local::rank_me(); i < proc::rank_n(); i += local::rank_n()) {
      vector<pair<char*, int>> results = amff_agg_buffer_p[i].flush();
      for (auto result: results) {
        if (get<0>(result) != nullptr) {
          if (get<1>(result) != 0) {
            gex_AM_RequestMedium0(backend::tm, i, am_internal::hidx_generic_am_ff_reqhandler,
                                  get<0>(result), get<1>(result), GEX_EVENT_NOW, 0);
          }
          delete [] get<0>(result);
        }
      }
    }
  }
}
} // namespace am_internal

template <typename Fn, typename... Args>
void rpc_ff(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = tuple<Args...>;
  using am_internal::AmffTypeWrapper;
  using am_internal::AmffReqMeta;

  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();

  intptr_t fn_p = am_internal::get_pi_fnptr(&fn);
  intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmffTypeWrapper<remove_reference_t<Fn>, Args...>::invoker);
  AmffReqMeta meta{fn_p, wrapper_p, remote_worker_local};
  Payload payload(forward<Args>(args)...);
//  pair<am_internal::AmffReqMeta, Payload> data{meta, payload};

  pair<char*, int> result = am_internal::amff_agg_buffer_p[remote_proc].push(meta, std::move(payload));
  if (get<0>(result) != nullptr) {
    if (get<1>(result) != 0) {
      gex_AM_RequestMedium0(backend::tm, remote_proc, am_internal::hidx_generic_am_ff_reqhandler,
                            get<0>(result), get<1>(result), GEX_EVENT_NOW, 0);
    }
    delete [] get<0>(result);
  }
  ++(*am_internal::am_req_counter);
}

} // namespace arl

#endif //ARL_AM_FF_HPP
