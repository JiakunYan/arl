#define ARL_PROFILE
#include "arl/arl.hpp"
#include "arl/tool/utils.hpp"
#include <cassert>
#include "external/cxxopts.hpp"

bool use_agg = false;
size_t agg_size = 0;

void empty_handler() {
  // do nothing
}

void worker() {

  int num_ops = 100000;
  int total_num_ops = num_ops * (int) arl::rank_n();
  double ticks_step = 0;
#ifdef ARL_PROFILE
  arl::SharedTimer timer_rand;
  arl::SharedTimer timer_rpc;
  arl::SharedTimer timer_push;
  arl::SharedTimer timer_barrier;
  arl::SharedTimer timer_get;
#endif
  size_t my_rank = arl::rank_me();
  size_t nworkers = arl::rank_n();

  using rv = decltype(arl::rpc(size_t(), empty_handler));
  std::vector<rv> futures;

  arl::barrier();
  arl::tick_t start = arl::ticks_now();

  for (int i = 0; i < num_ops; i++) {
#ifdef ARL_PROFILE
    timer_rand.start();
#endif
    size_t target_rank = lrand48() % nworkers;
#ifdef ARL_PROFILE
    timer_rand.end_and_update();
    timer_rpc.start();
#endif
    rv f;
    if (use_agg) {
      f = arl::rpc_agg(target_rank, empty_handler);
    } else {
      f = arl::rpc(target_rank, empty_handler);
    }
#ifdef ARL_PROFILE
    timer_rpc.end_and_update();
    timer_push.start();
#endif
    futures.push_back(std::move(f));
#ifdef ARL_PROFILE
    timer_push.end_and_update();
#endif
  }

#ifdef ARL_PROFILE
  timer_barrier.start();
#endif
  // start arl::barrier()
  arl::threadBarrier.wait();
  arl::flush_agg_buffer();
  arl::tick_t end_req = arl::ticks_now();
  arl::flush_am();
  if (arl::local::rank_me() == 0) {
    arl::backend::barrier();
  }
  arl::threadBarrier.wait();
  // end arl::barrier()
#ifdef ARL_PROFILE
  timer_barrier.end_and_update();
#endif

  for (int i = 0; i < num_ops; i++) {
#ifdef ARL_PROFILE
    timer_get.start();
#endif
    futures[i].get();
#ifdef ARL_PROFILE
    timer_get.end_and_update();
#endif
  }

  arl::barrier();
  arl::tick_t end_wait = arl::ticks_now();

  double duration_req = arl::ticks_to_ns(end_req - start) / 1e3;
  double agg_overhead = duration_req / num_ops * MAX(agg_size, 1);
  double ave_overhead = duration_req / num_ops;
  double duration_total = arl::ticks_to_ns(end_wait - start) / 1e3;
  arl::print("Setting: agg_size = %lu; duration = %.2lf s; num_ops = %lu\n", agg_size, duration_total / 1e6, num_ops);
  arl::print("ave_overhead: %.2lf us; agg_overhead: %.2lf us\n", ave_overhead, agg_overhead);
  arl::print("Total throughput: %lu op/s\n", (unsigned long) (num_ops / (duration_req / 1e6)));
#ifdef ARL_PROFILE
  // fine-grained
  arl::print("rand: %.3lf us\n", timer_rand.to_us());
  arl::print("rpc/rpc_agg: %.3lf us\n", timer_rpc.to_us());
  arl::print("push: %.3lf us\n", timer_push.to_us());
  arl::print("barrier: %.3lf us (ave: %.3lf us)\n", timer_barrier.to_us(), timer_barrier.to_us() / num_ops);
  arl::print("get: %.3lf us\n", timer_get.to_us());
  // rpc backend
  arl::print("rpc/future preparation: %.3lf us\n", arl::timer_load.to_us());
  arl::print("agg buffer without pop: %.3lf us\n", arl::timer_buf_npop.to_us());
  arl::print("agg buffer with pop: %.3lf us\n", arl::timer_buf_pop.to_us());
  arl::print("agg buffer ave: %.3lf us\n", (arl::timer_buf_pop.to_us() + arl::timer_buf_npop.to_us() * MAX(agg_size - 1, 0)) / MAX(agg_size, 1));
  arl::print("Gasnet_ex req: %.3lf us (ave: %.3lf us)\n", arl::timer_gex_req.to_us(), arl::timer_gex_req.to_us() / agg_size);
#endif
}

int main(int argc, char** argv) {
  // one process per node
  cxxopts::Options options("ARL Benchmark", "Benchmark of ARL system");
  options.add_options()
      ("size", "Aggregation size", cxxopts::value<size_t>())
      ;
  auto result = options.parse(argc, argv);
  try {
    agg_size = result["size"].as<size_t>();
  } catch (...) {
    agg_size = 0;
  }
  assert(agg_size >= 0);
  use_agg = (agg_size != 0);

  arl::init(15, 16);
  if (use_agg) {
    agg_size = arl::set_agg_size(agg_size);
  }

  arl::run(worker);

  arl::finalize();
}
