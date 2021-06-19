//
// Created by jackyan on 4/3/20.
//

#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <upcxx/upcxx.hpp>
#include "gex_timer.hpp"

using namespace upcxx;

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
int worker(int compute_us) {
  int num_ops = 10000;
  std::default_random_engine generator(rank_me());
  std::uniform_int_distribution<int> distribution(0, rank_n()-1);
  arl::SimpleTimer timer_total;
  ReqPayload<REQ_N> payload{};
  barrier();
  timer_total.start();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    arl::usleep(compute_us);
    upcxx::rpc(target_rank, empty_handler<REQ_N, ACK_N>, payload).wait();
  }
  barrier();
  timer_total.end();

  double duration_s = timer_total.to_s();
  if (!rank_me()) {
    printf("compute time is %d us\n", compute_us);
    printf("rpc latency is %.2lf us (total %.2lf s)\n", timer_total.to_us() / num_ops, timer_total.to_s());
  }
  return 0;
}

int main() {
  upcxx::init();
  worker<64, 8>(0);
  worker<64, 8>(10);
  worker<64, 8>(32);
  worker<64, 8>(100);
  worker<64, 8>(320);
  worker<64, 8>(1000);
  upcxx::finalize();
}