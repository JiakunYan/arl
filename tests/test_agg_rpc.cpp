#define ARL_DEBUG
#include "arl/arl.hpp"
#include <cassert>

// test Aggregation

void worker() {

  int my_rank = (int) arl::rank_me();
  int steps = 1000;

  auto fn = [](int a, int b) -> int {
    return a * b;
  };

  using rv = decltype(arl::rpc(0, fn, my_rank, my_rank));
  std::vector<rv> futures;

  for (int i = 0 ; i < steps; i++) {
    size_t target_rank = rand() % arl::rank_n();
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
  arl::set_agg_size(50);

  arl::run(worker);

  arl::finalize();
}
