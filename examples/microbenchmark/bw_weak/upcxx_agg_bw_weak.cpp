//
// Created by jackyan on 4/3/20.
//

#define MAX_RPCS_IN_FLIGHT 8192
#define MIN_INFLIGHT_BYTES 4194304
#define ONE_MB (1024*1024)
#define ONE_GB (ONE_MB*1024)
#define MAX_FILE_PATH 255
#define MAX_RANKS_PER_DIR 1024
#define USE_COLORS

#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include "../aggr_store.hpp"
#include "../gex_timer.hpp"

using namespace upcxx;

const int aggr_size = 50 * 1024 * 1024; // 50MB

template<int N>
struct Payload {
  char data[N];
};

template<int N>
struct EmptyFn {
  void operator()(Payload<N> &payload) {
  }
};

template<int N>
int worker() {
  AggrStore<Payload<N>> aggrStore;
  aggrStore.set_size("bw", aggr_size);

  dist_object<EmptyFn<N>> empty_fn(world());

  int num_ops = 1000000;
  std::default_random_engine generator(rank_me());
  std::uniform_int_distribution<int> distribution(0, rank_n()-1);
  arl::SimpleTimer timer_total;
  Payload<N> payload{};
  barrier();
  timer_total.start();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);

    aggrStore.update(target_rank, payload, empty_fn);
  }
  aggrStore.flush_updates(empty_fn);
  barrier();
  auto end = std::chrono::high_resolution_clock::now();
  timer_total.end();

  double duration_s = timer_total.to_s();
  double bandwidth_node_s = (double) N * num_ops * 32 / duration_s;
  if (!rank_me()) {
    printf("req payload size = %lu Byte\n", sizeof(Payload<N>));
    printf("aggr_store overhead is %.2lf us (total %.2lf s)\n", timer_total.to_us() / num_ops, timer_total.to_s());
    printf("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", bandwidth_node_s / 1e6);
  }
//  if (!rank_me()) {
//    printf("total time is %.2lf us\n", timer.to_us() / num_ops);
//  }
  aggrStore.clear();
  return 0;
}

int main() {
  upcxx::init();
  worker<8>();
  worker<16>();
  worker<32>();
  worker<48>();
  worker<64>();
  worker<128>();
  upcxx::finalize();
}