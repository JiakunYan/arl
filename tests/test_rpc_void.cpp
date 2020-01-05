#define ARL_DEBUG
#include "arl/arl.hpp"
#include <cassert>

void worker() {
  int my_rank = (int) arl::my_worker();

  auto fn = [](int a) {};

  using rv = decltype(arl::rpc(0, fn, my_rank));
  std::vector<rv> futures;

  for (int i = 0 ; i < 10; i++) {
    size_t target_rank = rand() % arl::nworkers();
    auto f = arl::rpc(target_rank, fn, my_rank);
    futures.push_back(std::move(f));
  }

  for (auto& f : futures) {
    f.get();
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init();

  arl::run(worker);

  arl::finalize();
}
