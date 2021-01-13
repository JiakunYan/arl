#define ARL_PROFILE
#include "arl.hpp"
#include <cassert>
#include "external/cxxopts.hpp"

using namespace arl;

bool use_agg = false;
size_t aggr_size = 65536;

void empty_handler() {
  // do nothing
}

void worker() {

  int num_ops = 1000000;
  int total_num_ops = num_ops * (int) rank_n();
  double ticks_step = 0;
#ifdef ARL_PROFILE
  SimpleTimer timer_rand;
  SimpleTimer timer_rpc;
  SimpleTimer timer_push;
  SimpleTimer timer_barrier;
  SimpleTimer timer_get;
#endif
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();

  using rv = decltype(rpc(size_t(), empty_handler));
  std::vector<rv> futures;

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
#ifdef ARL_PROFILE
    timer_rand.start();
#endif
    size_t target_rank = lrand48() % nworkers;
#ifdef ARL_PROFILE
    timer_rand.end();
    timer_rpc.start();
#endif
    rv f;
    if (use_agg) {
      f = rpc_agg(target_rank, empty_handler);
    } else {
      f = rpc(target_rank, empty_handler);
    }
#ifdef ARL_PROFILE
    timer_rpc.end();
    timer_push.start();
#endif
    futures.push_back(std::move(f));
#ifdef ARL_PROFILE
    timer_push.end();
#endif
  }

#ifdef ARL_PROFILE
  timer_barrier.start();
#endif
  // start barrier()
  threadBarrier.wait();
  flush_agg_buffer();
  wait_am();
  if (local::rank_me() == 0) {
    proc::barrier();
  }
  threadBarrier.wait();
  // end barrier()
#ifdef ARL_PROFILE
  timer_barrier.end();
#endif

  for (int i = 0; i < num_ops; i++) {
#ifdef ARL_PROFILE
    timer_get.start();
#endif
    futures[i].get();
#ifdef ARL_PROFILE
    timer_get.end();
#endif
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_ns(end_wait - start) / 1e3;
  print("Setting: aggr_size = %lu (%lu B); duration = %.2lf s; num_ops = %lu\n", aggr_size, aggr_size * sizeof(rpc_t), duration_total / 1e6, num_ops);
  print("Total single-direction node bandwidth: %.2lf MB/s\n", (sizeof(rpc_t) * num_ops * local::rank_n() * 2 / duration_total));
#ifdef ARL_PROFILE
  // fine-grained
  print("rand: %.3lf us\n", timer_rand.to_us());
  print("rpc/rpc_agg: %.3lf us\n", timer_rpc.to_us());
  print("push: %.3lf us\n", timer_push.to_us());
  print("barrier: %.3lf us (ave: %.3lf us)\n", timer_barrier.to_us(), timer_barrier.to_us() / num_ops);
  print("get: %.3lf us\n", timer_get.to_us());
  // rpc backend
  print("rpc/future preparation: %.3lf us\n", timer_load.to_us());
  print("agg buffer push (one): %.3lf us\n", timer_buf_push_one.to_us());
  print("agg buffer push (all): %.3lf us\n", timer_buf_push_all.to_us());
  print("agg buffer pop (one): %.3lf us\n", timer_buf_pop_one.to_us());
  print("agg buffer pop (amortized): %.3lf us\n", timer_buf_pop_ave.to_us());
//  print("agg buffer without pop: %.3lf us\n", timer_buf_npop.to_us());
//  print("agg buffer with pop: %.3lf us\n", timer_buf_pop.to_us());
//  print("agg buffer ave: %.3lf us\n", (timer_buf_pop.to_us() + timer_buf_npop.to_us() * MAX(aggr_size - 1, 0)) / MAX(aggr_size, 1));
  print("Gasnet_ex req: %.3lf us (ave: %.3lf us)\n", timer_gex_req.to_us(), timer_gex_req.to_us() / aggr_size);
#endif
}

int main(int argc, char** argv) {
  // one process per node
  cxxopts::Options options("ARH Benchmark", "Benchmark of ARH system");
  options.add_options()
      ("size", "Aggregation size", cxxopts::value<size_t>()->default_value("65536"))
      ;
  auto result = options.parse(argc, argv);
  aggr_size = result["size"].as<size_t>();

  assert(aggr_size >= 0);
  use_agg = (aggr_size != 0);

  init(15, 16, 2048);
  if (use_agg) {
    aggr_size = set_agg_size(aggr_size);
  }

  run(worker);

  finalize();
}
