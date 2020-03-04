#ifndef ARL_AM_AGGREGATE_HPP
#define ARL_AM_AGGREGATE_HPP

#include <vector>
using std::unique_ptr;
using std::make_unique;

namespace arl {
#ifdef ARL_PROFILE
  alignas(alignof_cacheline) arl::SharedTimer timer_load;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_pop_one;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_pop_ave;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_push_one;
  alignas(alignof_cacheline) arl::SharedTimer timer_buf_push_all;
#endif
  alignas(alignof_cacheline) std::vector<AggBuffer> agg_buffers;
  alignas(alignof_cacheline) size_t max_agg_size;
  alignas(alignof_cacheline) std::atomic<size_t> agg_size;

  void init_agg() {
    max_agg_size = std::min(
        gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0),
        gex_AM_MaxReplyMedium  (backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0)
        );
    agg_size = max_agg_size;

    agg_buffers = std::vector<AggBuffer>(proc::rank_n());
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      agg_buffers[i].init<rpc_t>(agg_size);
    }
  }

  size_t set_agg_size(size_t custom_agg_size) {
    ARL_Assert(custom_agg_size >= sizeof(rpc_t), "");
    ARL_Assert(custom_agg_size >= sizeof(rpc_result_t), "");
    agg_size = std::min(max_agg_size, custom_agg_size);
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      agg_buffers[i].init<rpc_t>(agg_size);
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
    unique_ptr<char[]> buffer;
    size_t len = agg_buffers[id].pop_all(buffer);
    if (len > 0) {
      generic_handler_request_impl_(id, buffer.get(), len);
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

#ifdef ARL_PROFILE
    timer_load.start();
#endif
    Future<std::invoke_result_t<Fn, Args...>> future;
    rpc_t my_rpc(future.get_p(), remote_worker_local);
    my_rpc.load(std::forward<Fn>(fn), std::forward<Args>(args)...);
#ifdef ARL_PROFILE
    timer_load.end_and_update();
#endif

#ifdef ARL_RPC_AS_LPC
    if (remote_proc == proc::rank_me()) {
      // lpc
      rpc_as_lpc(std::move(my_rpc));
    } else
#endif
    {
      // rpc
#ifdef ARL_PROFILE
      timer_buf_push_one.start();
      timer_buf_push_all.start();
#endif
      auto status = agg_buffers[remote_proc].push(my_rpc);
#ifdef ARL_PROFILE
      timer_buf_push_one.end_and_update();
#endif
      while (status == AggBuffer::status_t::FAIL) {
        progress();
        status = agg_buffers[remote_proc].push(my_rpc);
      }
#ifdef ARL_PROFILE
      timer_buf_push_all.end_and_update();
      timer_buf_pop_ave.start();
#endif
      if (status == AggBuffer::status_t::SUCCESS_AND_FULL) {
#ifdef ARL_PROFILE
        timer_buf_pop_one.start();
#endif
        flush_agg_buffer_single(remote_proc);
#ifdef ARL_PROFILE
        timer_buf_pop_one.end_and_update();
#endif
      }
#ifdef ARL_PROFILE
      timer_buf_pop_ave.end_and_update();
#endif
      requesteds[local::rank_me()].val++;
    }

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

#ifdef ARL_RPC_AS_LPC
    if (remote_proc == proc::rank_me()) {
      // lpc
      rpc_as_lpc(std::move(my_rpc));
    } else
#endif
    {
      // rpc
      auto status = agg_buffers[remote_proc].push(my_rpc);
      while (status == AggBuffer::status_t::FAIL) {
        progress();
        status = agg_buffers[remote_proc].push(my_rpc);
      }
      if (status == AggBuffer::status_t::SUCCESS_AND_FULL) {
        flush_agg_buffer_single(remote_proc);
      }
      requesteds[local::rank_me()].val++;
    }

    return;
  }
}

#endif //ARL_AM_AGGREGATE_HPP
