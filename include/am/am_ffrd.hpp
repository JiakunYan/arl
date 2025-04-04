#ifndef ARL_AM_FFRD_HPP
#define ARL_AM_FFRD_HPP

namespace arl {
namespace amffrd_internal {

using am_internal::AggBuffer;
using am_internal::get_pi_fnptr;
using am_internal::resolve_pi_fnptr;

// AM synchronous counters
extern AlignedAtomicInt64 *amffrd_recv_counter;
extern std::vector<std::vector<int64_t>> *amffrd_req_counters;
// other variables whose names are clear
extern AggBuffer **amffrd_agg_buffer_p;
extern uint8_t g_am_idx;
extern size_t g_payload_size;

void sendm_amffrd(rank_t remote_proc, void *buf, size_t nbytes);
}// namespace amffrd_internal

template<typename Fn, typename... Args>
void register_amffrd(Fn&& fn) {
  pure_barrier();
  if (local::rank_me() == 0) {
    amffrd_internal::g_am_idx = am_internal::register_amhandler(std::forward<Fn>(fn));
    amffrd_internal::g_payload_size = am_internal::function_traits<std::decay_t<Fn>>::total_arg_size;
  }
  pure_barrier();
}

template<typename Fn, typename... Args>
void register_amffrd(Fn&& fn, Args&&...) {
  static_assert(std::is_invocable_v<Fn, Args...>,
                "register_amffrd: fn must be callable with Args");
  pure_barrier();
  if (local::rank_me() == 0) {
    amffrd_internal::g_am_idx = am_internal::register_amhandler(std::forward<Fn>(fn));
    amffrd_internal::g_payload_size = am_internal::function_traits<std::decay_t<Fn>>::total_arg_size;
  }
  pure_barrier();
}

// TODO: memcpy on tuple is dangerous
template<typename Fn, typename... Args>
void rpc_ffrd(rank_t remote_worker, Fn && fn, Args &&...args) {
  ARL_Assert(amffrd_internal::g_am_idx != -1, "call rpc_ffrd before register_ffrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
  if (config::rpc_loopback && remote_proc == proc::rank_me()) {
    // local precedure call
    timer_backup.end();
    amff_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
    timer_backup.start();
    return;
  }

  auto *aggbuffer = amffrd_internal::amffrd_agg_buffer_p[remote_proc];
  char *buf = nullptr;
  size_t nbytes = am_internal::AmHandlerRegistry::sizeof_args<Args...>();
  std::pair<char *, int64_t> result = aggbuffer->reserve(nbytes, &buf);
  am_internal::AmHandlerRegistry::serialize_args(buf, std::forward<Args>(args)...);
  aggbuffer->commit(nbytes);
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      timer_backup.end();
      amffrd_internal::sendm_amffrd(remote_proc, std::get<0>(result), std::get<1>(result));
      progress_external();
      timer_backup.start();
    }
  }
  ++(*amffrd_internal::amffrd_req_counters)[local::rank_me()][remote_proc];
}

}// namespace arl

#endif//ARL_AM_FFRD_HPP
