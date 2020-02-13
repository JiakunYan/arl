#ifndef ARL_AM_AGGREGATE_HPP
#define ARL_AM_AGGREGATE_HPP

#include <vector>

namespace arl {
#ifdef ARL_PROFILE
  alignas(alignof_cacheline) arl::SharedTimer timer_load;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_npop;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_pop;
  alignas(alignof_cacheline) arl::SharedTimer timer_gex_req;
#endif
  alignas(alignof_cacheline) std::vector<AggBuffer<rpc_t>> agg_buffers;
  alignas(alignof_cacheline) size_t max_agg_size;
  alignas(alignof_cacheline) std::atomic<size_t> agg_size;

  void init_agg() {
    max_agg_size = std::min(
        gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0) / sizeof(rpc_t),
        gex_AM_MaxReplyMedium  (backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0) / sizeof(rpc_result_t)
        );
    agg_size = max_agg_size;

    agg_buffers = std::vector<AggBuffer<rpc_t>>(proc::rank_n());
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      agg_buffers[i].init(agg_size);
    }
  }

  size_t set_agg_size(size_t custom_agg_size) {
    ARL_Assert(custom_agg_size > 0, "");
    agg_size = std::min(max_agg_size, custom_agg_size);
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      agg_buffers[i].init(agg_size);
    }
    return agg_size.load();
  }

  size_t get_max_agg_size() {
    return max_agg_size;
  }

  size_t get_agg_size() {
    return agg_size.load();
  }

  void flush_agg_buffer_single(size_t id) {
    std::vector<rpc_t> send_buf;
    size_t len = agg_buffers[id].pop_all(send_buf);
    if (len > 0) {
      generic_handler_request_impl_(id, std::move(send_buf));
    }
  }

  void flush_agg_buffer() {
    for (size_t i = 0; i < proc::rank_n(); i++) {
      flush_agg_buffer_single(i);
    }
  }

  template <typename Fn, typename... Args>
  Future<std::invoke_result_t<Fn, Args...>>
  rpc_agg(size_t remote_worker, Fn&& fn, Args&&... args) {
    ARL_Assert(remote_worker < rank_n(), "");

    rank_t remote_proc = remote_worker / local::rank_n();
    u_int8_t remote_worker_local = (u_int8_t) remote_worker % local::rank_n();

    Future<std::invoke_result_t<Fn, Args...>> future;
    rpc_t my_rpc(future.get_p(), remote_worker_local);
    my_rpc.load(std::forward<Fn>(fn), std::forward<Args>(args)...);
    auto status = agg_buffers[remote_proc].push(std::move(my_rpc));
    while (status == AggBuffer<rpc_t>::status_t::FAIL) {
      progress();
      status = agg_buffers[remote_proc].push(std::move(my_rpc));
    }
    if (status == AggBuffer<rpc_t>::status_t::SUCCESS_AND_FULL) {
      flush_agg_buffer_single(remote_proc);
    }
    requesteds[local::rank_me()].val++;

    return std::move(future);
  }

  template <typename Fn, typename... Args>
  void rpc_ff(size_t remote_worker, Fn&& fn, Args&&... args) {
    static_assert(std::is_same<std::invoke_result_t<Fn, Args...>, void>::value, "rpc_ff must return void!");
    ARL_Assert(remote_worker < rank_n(), "");

    rank_t remote_proc = remote_worker / local::rank_n();
    u_int8_t remote_worker_local = (u_int8_t) remote_worker % local::rank_n();

    rpc_t my_rpc(NULL, remote_worker_local);
    my_rpc.load(std::forward<Fn>(fn), std::forward<Args>(args)...);
    auto status = agg_buffers[remote_proc].push(std::move(my_rpc));
    while (status == AggBuffer<rpc_t>::status_t::FAIL) {
      progress();
      status = agg_buffers[remote_proc].push(std::move(my_rpc));
    }
    if (status == AggBuffer<rpc_t>::status_t::SUCCESS_AND_FULL) {
      flush_agg_buffer_single(remote_proc);
    }
    requesteds[local::rank_me()].val++;

    return;
  }
}

#endif //ARL_AM_AGGREGATE_HPP
