//
// Created by jiakun on 2020/6/27.
//
#include <iostream>
#include <vector>
#include <random>
#include <upcxx/upcxx.hpp>
#include "examples/microbenchmark/aggr_store.hpp"
#include "examples/microbenchmark/gex_timer.hpp"

using namespace std;

const int64_t COUNTER_RANGE = 1e6;
const int64_t NUM_OP = 1e6;
vector<int64_t>* counter_p;
int64_t counter_size;

upcxx::intrank_t val_to_rank(int64_t val) {
  return val / counter_size;
}

int64_t val_to_index(int64_t val) {
  return val % counter_size;
}

int64_t index_to_val(int64_t index) {
  return counter_size * upcxx::rank_me() + index;
}

void counter_init() {
  counter_size = (COUNTER_RANGE + upcxx::rank_n() - 1) / upcxx::rank_n();
  counter_p = new vector<int64_t>(counter_size);
  for (auto & counter: *counter_p) {
    counter = 0;
  }
}

void counter_fina() {
  delete counter_p;
}

void print_distribution() {
  const int interval_num = 5;
  vector<int64_t> counter_interval(interval_num, 0);
  int64_t interval_range = (COUNTER_RANGE + interval_num - 1) / interval_num;
  int64_t max_freq = -1, max_index = -1;
  for (int64_t i = 0; i < counter_p->size(); ++i) {
    int64_t count = (*counter_p)[i];
    int64_t val = index_to_val(i);
    if (count > max_freq) {
      max_freq = count;
      max_index = val;
    }
    counter_interval[val / interval_range] += count;
  }
  int64_t global_max_freq = upcxx::reduce_all(max_freq, upcxx::op_fast_max).wait();
  if (global_max_freq == max_freq) {
    printf("Max frequency: %ld occurs %ld times\n", max_index, max_freq);
    printf("Average frequency: %ld times\n", NUM_OP * upcxx::rank_n() / COUNTER_RANGE);
  }
  vector<int64_t> global_counter_interval(interval_num);
  upcxx::reduce_all(counter_interval.data(), global_counter_interval.data(),
                    interval_num, upcxx::op_fast_add).wait();
  if (upcxx::rank_me() == 0) {
    for (int i = 0; i < interval_num; ++i)
      printf("[%ld, %ld): %ld\n", interval_range*i, interval_range*(i+1), global_counter_interval[i]);
  }
}
struct CountFn {
  void operator()(int64_t val) {
    int64_t index = val_to_index(val);
    (*counter_p)[index] += 1;
  }
};

void worker(double lambda) {
  default_random_engine generator(upcxx::rank_me());
  exponential_distribution<double> distribution(lambda);
  counter_init();

  AggrStore<int64_t> aggrStore;
  aggrStore.set_size("load_imbalance", 50 * 1024 * 1024);
  upcxx::dist_object<CountFn> count_fn(upcxx::world());
  arl::SimpleTimer timer;

  upcxx::barrier();
  timer.start();

  for (int64_t i = 0; i < NUM_OP; ++i) {
    auto val = (int64_t) (distribution(generator) * COUNTER_RANGE) % COUNTER_RANGE;
    upcxx::intrank_t target_rank = val_to_rank(val);
    aggrStore.update(target_rank, val, count_fn);
  }
  aggrStore.flush_updates(count_fn);

  upcxx::barrier();
  timer.end();

//  print_distribution();
  if (!upcxx::rank_me()) {
    printf("lambda: %.2lf; total time: %.2lf s\n", lambda, timer.to_s());
    fflush(stdout);
  }

  upcxx::barrier();
  aggrStore.clear();
  counter_fina();
}

int main(int argc, char** argv) {
  upcxx::init();
  worker(1);
  worker(2);
  worker(4);
  worker(8);
  worker(16);
  worker(32);
  worker(64);
  worker(128);
  worker(256);
  worker(512);
  worker(1024);
  worker(2048);
  worker(4096);
  upcxx::finalize();
}