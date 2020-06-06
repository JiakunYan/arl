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
#include "examples/microbenchmark/aggr_store.hpp"
#include "examples/microbenchmark/gex_timer.hpp"

using namespace upcxx;

const int aggr_size = 50 * 1024 * 1024; // 50MB

template<int N>
struct Payload {
  char data[N];
};

struct SleepPayload {
  int64_t sleep_time;
  Payload<56> useless;
};

struct SleepFn {
  void operator()(SleepPayload payload) {
    // do nothing
    arl::usleep(payload.sleep_time);
  }
};

int worker(int64_t sleep_us) {
  AggrStore<SleepPayload> aggrStore;
  aggrStore.set_size("bw", aggr_size);

  dist_object<SleepFn> sleep_fn(world());

  int num_ops;
  if (sleep_us > 0)
    num_ops = 1000000 / sleep_us;
  else
    num_ops = 1000000;
  std::default_random_engine generator(rank_me());
  std::uniform_int_distribution<int> distribution(0, rank_n()-1);
  arl::SimpleTimer timer_total;
  SleepPayload sleep_payload{sleep_us, Payload<56>{}};
  barrier();
  timer_total.start();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);

    aggrStore.update(target_rank, sleep_payload, sleep_fn);
  }
  aggrStore.flush_updates(sleep_fn);
  barrier();
  timer_total.end();

  double duration_s = timer_total.to_s();
  if (!rank_me()) {
    printf("sleep time is %ld us\n", sleep_us);
    printf("aggr_store overhead is %.2lf us (total %.2lf s)\n", timer_total.to_us() / num_ops, timer_total.to_s());
  }
//  if (!rank_me()) {
//    printf("total time is %.2lf us\n", timer.to_us() / num_ops);
//  }
  aggrStore.clear();
  return 0;
}

int main() {
  upcxx::init();
  worker(0);
  worker(1);
  worker(10);
  worker(30);
  worker(100);
  worker(300);
  worker(1000);
  upcxx::finalize();
}