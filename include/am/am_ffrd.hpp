#ifndef ARL_AM_FFRD_HPP
#define ARL_AM_FFRD_HPP

namespace arl {
namespace amffrd_internal {

using am_internal::AggBuffer;
using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;

// AM synchronous counters
extern AlignedAtomicInt64 *amffrd_recv_counter;
extern AlignedAtomicInt64 *amffrd_req_counters;// of length proc::rank_n()
// other variables whose names are clear
extern AggBuffer **amffrd_agg_buffer_p;

struct AmffrdReqMeta {
  intptr_t fn_p;
  intptr_t type_wrapper_p;
};

extern AmffrdReqMeta *global_meta_p;

template<typename... Args>
struct AmffrdReqPayload {
  int target_local_rank;
  std::tuple<Args...> data;
};

template<typename Fn, typename... Args>
class AmffrdTypeWrapper {
  public:
  static intptr_t invoker(const std::string &cmd) {
    if (cmd == "req_invoker") {
      return get_pi_fnptr(req_invoker);
    } else {
      throw std::runtime_error("Unknown function call");
    }
  }

  static int req_invoker(intptr_t fn_p, char *buf, int nbytes) {
    static_assert(std::is_void_v<Result>);
    ARL_Assert(nbytes >= (int) sizeof(Payload), "(", nbytes, " >= ", sizeof(Payload), ")");
    ARL_Assert(nbytes % (int) sizeof(Payload) == 0, "(", nbytes, " % ", sizeof(Payload), " == 0)");
    //    printf("req_invoker: fn_p %ld, buf %p, nbytes %d, sizeof(Payload) %lu\n", fn_p, buf, nbytes, sizeof(Payload));
    Fn *fn = resolve_pi_fnptr<Fn>(fn_p);
    auto ptr = reinterpret_cast<Payload *>(buf);

    for (int i = 0; i < nbytes; i += sizeof(Payload)) {
      Payload &payload = *reinterpret_cast<Payload *>(buf + i);
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

  template<size_t... I>
  static void run_fn(Fn *fn, const std::tuple<Args...> &data, std::index_sequence<I...>) {
    std::invoke(*fn, std::get<I>(data)...);
  }
};

extern void sendm_amffrd(rank_t remote_proc, AmffrdReqMeta meta, void *buf, size_t nbytes);
}// namespace amffrd_internal

// TODO: handle function pointer situation
template<typename Fn, typename... Args>
void register_amffrd(const Fn &fn) {
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
template<typename Fn, typename... Args>
void register_amffrd(Fn &&fn, Args &&...) {
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
template<typename Fn, typename... Args>
void rpc_ffrd(rank_t remote_worker, Fn &&fn, Args &&...args) {
  using Payload = amffrd_internal::AmffrdReqPayload<std::remove_reference_t<Args>...>;
  using amffrd_internal::AmffrdReqMeta;
  using amffrd_internal::AmffrdTypeWrapper;

  ARL_Assert(amffrd_internal::global_meta_p != nullptr, "call rpc_ffrd before register_ffrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  if (remote_proc == proc::rank_me()) {
    // local precedure call
    return amff_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
    // Fn* fn_p = am_internal::resolve_pi_fnptr<Fn>(amffrd_internal::global_meta_p->fn_p);
    // amff_internal::run_lpc(remote_worker_local, *fn_p, std::forward<Args>(args)...);
    return;
  }

  Payload payload{remote_worker_local, std::make_tuple(std::forward<Args>(args)...)};
  //  printf("Rank %ld send rpc to rank %ld\n", rank_me(), remote_worker);
  //  std::cout << "sizeof(" << type_name<Payload>() << ") is " << sizeof(Payload) << std::endl;

  std::pair<char *, int64_t> result = amffrd_internal::amffrd_agg_buffer_p[remote_proc]->push((char *) &payload, sizeof(payload));
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      amffrd_internal::sendm_amffrd(remote_proc, *amffrd_internal::global_meta_p,
                                    std::get<0>(result), std::get<1>(result));
      progress_external();
    }
  }
  ++(amffrd_internal::amffrd_req_counters[remote_proc].val);
}

}// namespace arl

#endif//ARL_AM_FFRD_HPP
