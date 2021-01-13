//
// Created by jiakun on 2020/6/27.
//
#include <iostream>
#include <vector>
#include <random>
#include "arl.hpp"

using namespace std;

const int64_t COUNTER_RANGE = 1e6;
const int64_t NUM_OP = 1e6;
vector<atomic<int64_t>>* counter_p;
int64_t counter_size;

arl::rank_t val_to_rank(int64_t val) {
  return val / counter_size * arl::local::rank_n();
}

int64_t val_to_index(int64_t val) {
  return val % counter_size;
}

int64_t index_to_val(int64_t index) {
  return counter_size * arl::proc::rank_me() + index;
}

void counter_init() {
  counter_size = (COUNTER_RANGE + arl::proc::rank_n() - 1) / arl::proc::rank_n();
  counter_p = new vector<atomic<int64_t>>(counter_size);
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
  int64_t global_max_freq = arl::proc::reduce_all(max_freq, arl::op_max());
  if (global_max_freq == max_freq) {
    printf("Max frequency: %ld occurs %ld times\n", max_index, max_freq);
    printf("Average frequency: %ld times\n", NUM_OP * arl::rank_n() / COUNTER_RANGE);
  }
  vector<int64_t> global_counter_interval = arl::proc::reduce_all(counter_interval, arl::op_plus());
  if (arl::proc::rank_me() == 0) {
    for (int i = 0; i < interval_num; ++i)
      printf("[%ld, %ld): %ld\n", interval_range*i, interval_range*(i+1), global_counter_interval[i]);
  }
}

void fn(int64_t val) {
  int64_t index = val_to_index(val);
  (*counter_p)[index] += 1;
}

void worker(double lambda) {
  default_random_engine generator(arl::rank_me());
  exponential_distribution<double> distribution(lambda);
  if (arl::local::rank_me() == 0)
    counter_init();
  arl::SimpleTimer timer;

  arl::register_amffrd(fn, int64_t());

  arl::barrier();
  timer.start();

  for (int64_t i = 0; i < NUM_OP; ++i) {
    auto val = (int64_t) (distribution(generator) * COUNTER_RANGE) % COUNTER_RANGE;
    arl::rank_t target_rank = val_to_rank(val);
    arl::rpc_ffrd(target_rank, fn, val);
  }

  arl::barrier();
  timer.end();

//  if (arl::local::rank_me() == 0) {
//    print_distribution();
//  }
  arl::print("lambda: %.2lf; total time: %.2lf s\n", lambda, timer.to_s());

  arl::barrier();
  if (arl::local::rank_me() == 0)
    counter_fina();
}

int main(int argc, char** argv) {
  arl::init(15, 16);
  arl::run(worker, 1);
  arl::run(worker, 2);
  arl::run(worker, 4);
  arl::run(worker, 8);
  arl::run(worker, 16);
  arl::run(worker, 32);
  arl::run(worker, 64);
  arl::run(worker, 128);
  arl::run(worker, 256);
  arl::run(worker, 512);
  arl::run(worker, 1024);
  arl::run(worker, 2048);
  arl::run(worker, 4096);
  arl::finalize();
}