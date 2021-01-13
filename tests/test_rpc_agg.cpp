#define ARL_DEBUG
#include "arl.hpp"
#include <cassert>
#include <random>

// test Aggregation

void worker() {

  int my_rank = (int) arl::rank_me();
  int steps = 1000;
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, arl::rank_n()-1);

  auto fn = [](int a, int b) -> int {
    return a * b;
  };

  using rv = decltype(arl::rpc_agg(0, fn, my_rank, my_rank));
  std::vector<rv> futures;

  for (int i = 0 ; i < steps; i++) {
    size_t target_rank = distribution(generator);
    auto f = arl::rpc_agg(target_rank, fn, my_rank, my_rank);
    futures.push_back(std::move(f));
  }

  arl::barrier();

  for (int i = 0 ; i < steps; i++) {
    int val = futures[i].get();
    assert(val == my_rank*my_rank);
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init();

  arl::run(worker);

  arl::finalize();
}
