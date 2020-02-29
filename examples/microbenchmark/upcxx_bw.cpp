//
// Created by jackyan on 2/26/20.
//

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <set>
#include <numeric>
#include <cstddef>
#include <chrono>
#include <upcxx/upcxx.hpp>
#include <random>

const size_t payload_size = 64 * 1024; // 64 KB
struct Payload {
  char data[payload_size] = {0};
};

void do_nothing(Payload data) {}

int main(int argc, char **argv) {
  upcxx::init();
  size_t num_ops = 10000;
  Payload payload;

  upcxx::intrank_t rank_me = upcxx::rank_me();
  upcxx::intrank_t rank_n = upcxx::rank_n();
  std::default_random_engine generator(rank_me);
  std::uniform_int_distribution<int> distribution(0, rank_n-1);

  upcxx::barrier();
  auto begin = std::chrono::high_resolution_clock::now();

  upcxx::future<> fut_all = upcxx::make_future();
  for (size_t i = 0; i < num_ops; ++i) {
    size_t remote_proc = distribution(generator);
    auto fut = upcxx::rpc(remote_proc, do_nothing, payload);
    fut_all = upcxx::when_all(fut_all, fut);
  }
  fut_all.wait();

  upcxx::barrier();
  auto end = std::chrono::high_resolution_clock::now();

  double duration_s = std::chrono::duration<double>(end - begin).count();
  double bandwidth_node_s = payload_size * num_ops * 32 / duration_s;
  if (!rank_me) {
    printf("payload size = %lu Byte\n", sizeof(Payload));
    printf("Node single-direction bandwidth = %.3lf MB/S\n", bandwidth_node_s / 1e6);
  }

  upcxx::finalize();
  return 0;
}
