//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AMFF_HPP
#define ARL_AMFF_HPP

namespace arl {
namespace amff_internal {

using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;
using am_internal::AggBuffer;

// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_recv_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_req_counters; // of length proc::rank_n()

alignas(alignof_cacheline) AggBuffer* amff_agg_buffer_p;


// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amff() {
  amff_agg_buffer_p = new AggBuffer[proc::rank_n()];

  int max_buffer_size = gex_AM_MaxRequestMedium(backend::internal::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_agg_buffer_p[i].init(max_buffer_size, 0);
  }
  amff_recv_counter = new AlignedAtomicInt64;
  amff_recv_counter->val = 0;
  amff_req_counters = new AlignedAtomicInt64[proc::rank_n()];
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_req_counters[i].val = 0;
  }
}

void exit_amff() {
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

    rank_t mContext = rank_internal::get_context();
    rank_internal::set_context(context);
    run_fn(fn, *ptr, std::index_sequence_for<Args...>());
    rank_internal::set_context(mContext);
  }

 private:
  using Payload = std::tuple<Args...>;
  using Result = std::invoke_result_t<Fn, Args...>;

  template <size_t... I>
  static void run_fn(Fn* fn, const Payload& data, std::index_sequence<I...>) {
    std::invoke(*fn,  std::get<I>(data)...);
  }
};

void generic_amff_reqhandler(const backend::cq_entry_t& event) {
  using payload_size_t = int();
  using req_invoker_t = void(intptr_t, int, char*, int);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_FF_REQ);
//  printf("rank %ld amff reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char* buf = (char*) event.buf;
  int nbytes = event.nbytes;
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

template <typename Fn, typename... Args>
void run_lpc(rank_t context, Fn&& fn, Args&&... args) {
  rank_t mContext = rank_internal::get_context();
  rank_internal::set_context(context);
  std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
  rank_internal::set_context(mContext);
}

void flush_amff_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char*, int64_t>> results = amff_agg_buffer_p[i].flush();
    for (auto result: results) {
      if ( std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          backend::sendm(i, am_internal::HandlerType::AM_FF_REQ, std::get<0>(result), std::get<1>(result));
          info::networkInfo.byte_send.add(std::get<1>(result));
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
      src_v[i] = amff_req_counters[i].val.load();
    }
    auto dst_v = proc::reduce_all(src_v, op_plus());
    expected_recv_num = dst_v[proc::rank_me()];
  }
  return local::broadcast(expected_recv_num, 0);
}

void wait_amff() {
  local::barrier();
  int64_t expected_recv_num = get_expected_recv_num();
  progress_until([&](){return expected_recv_num <= amff_recv_counter->val;});
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
  if (remote_proc == proc::rank_me()) {
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

  std::pair<char*, int64_t> result = amff_internal::amff_agg_buffer_p[remote_proc].push(meta, std::move(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      backend::sendm(remote_proc, am_internal::HandlerType::AM_FF_REQ, std::get<0>(result), std::get<1>(result));
      info::networkInfo.byte_send.add(std::get<1>(result));
      progress_external();
    }
    delete [] std::get<0>(result);
  }
  ++(amff_internal::amff_req_counters[remote_proc].val);
}

} // namespace arl

#endif //ARL_AMFF_HPP
