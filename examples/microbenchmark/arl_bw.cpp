#define ARL_PROFILE
#include "arl.hpp"
#include <cassert>
#include <random>

using namespace arl;
using namespace std;

template<int N>
struct ReqPayload {
  char data[N] = {0};
};

template<int N>
struct AckPayload {
  char data[N] = {0};
};

template<int REQ_N, int ACK_N>
AckPayload<ACK_N> empty_handler(ReqPayload<REQ_N> payload) {
  // do nothing
  return AckPayload<ACK_N>();
}

template<int REQ_N, int ACK_N>
void worker() {
#ifdef ARL_PROFILE
  SimpleTimer timer_rand;
  SimpleTimer timer_rpc;
  SimpleTimer timer_push;
  SimpleTimer timer_barrier;
  SimpleTimer timer_get;
#endif
  ReqPayload<REQ_N> req_payload;

  int num_ops = 1000000;
  int total_num_ops = num_ops * (int) rank_n();
  double ticks_step = 0;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  vector<Future<AckPayload<ACK_N>>> futures;
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
#ifdef ARL_PROFILE
    timer_rand.start();
#endif
    int target_rank = distribution(generator);
#ifdef ARL_PROFILE
    timer_rand.end();
    timer_rpc.start();
#endif
    auto future = rpc_agg(target_rank, empty_handler<REQ_N, ACK_N>, req_payload);
#ifdef ARL_PROFILE
    timer_rpc.end();
    timer_push.start();
#endif
    futures.push_back(move(future));
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

  double duration_total = ticks_to_us(end_wait - start);
  print("req payload size is %d Byte\n", REQ_N);
  print("ack payload size is %d Byte\n", ACK_N);
  print("rpc_agg overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (req/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggReqMeta) + sizeof(ReqPayload<REQ_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(ReqPayload<REQ_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggAckMeta) + sizeof(AckPayload<ACK_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/pure): %.2lf MB/s\n", ((sizeof(AckPayload<ACK_N>)) * num_ops * local::rank_n() * 2 / duration_total));

#ifdef ARL_PROFILE
  // fine-grained
  print("rand: %.3lf us\n", timer_rand.to_us());
  print("rpc/rpc_agg: %.3lf us\n", timer_rpc.to_us());
  print("push: %.3lf us\n", timer_push.to_us());
  print("barrier: %.3lf us (ave: %.3lf us)\n", timer_barrier.to_us(), timer_barrier.to_us() / num_ops);
  print("get: %.3lf us\n", timer_get.to_us());
  // rpc backend
//  print("rpc/future preparation: %.3lf us\n", timer_load.to_us());
//  print("agg buffer push (one): %.3lf us\n", timer_buf_push_one.to_us());
//  print("agg buffer push (all): %.3lf us\n", timer_buf_push_all.to_us());
//  print("agg buffer pop (one): %.3lf us\n", timer_buf_pop_one.to_us());
//  print("agg buffer pop (amortized): %.3lf us\n", timer_buf_pop_ave.to_us());
//  print("agg buffer without pop: %.3lf us\n", timer_buf_npop.to_us());
//  print("agg buffer with pop: %.3lf us\n", timer_buf_pop.to_us());
//  print("agg buffer ave: %.3lf us\n", (timer_buf_pop.to_us() + timer_buf_npop.to_us() * MAX(aggr_size - 1, 0)) / MAX(aggr_size, 1));
//  print("Gasnet_ex req: %.3lf us (ave: %.3lf us)\n", timer_gex_req.to_us(), timer_gex_req.to_us() / aggr_size);
#endif
}

int main(int argc, char** argv) {
  init(15, 16, 2048);

  run(worker<64, 8>);

  finalize();
}
