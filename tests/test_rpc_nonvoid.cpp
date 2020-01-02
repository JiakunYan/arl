  #include "arl/arl.hpp"
  #include <cassert>


void worker() {

  int my_rank = (int) arl::my_worker();

  auto fn = [](int a, int b) -> int {
    return a * b;
  };

  using rv = decltype(arl::rpc(0, fn, my_rank, my_rank));
  std::vector<rv> futures;

  for (int i = 0 ; i < 10; i++) {
    size_t target_rank = rand() % arl::nworkers();
    auto f = arl::rpc(target_rank, fn, my_rank, my_rank);
    futures.push_back(std::move(f));
  }

  for (auto& f : futures) {
    int val = f.get();
    assert(val == my_rank*my_rank);
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init();

  arl::run(worker);

  arl::finalize();
}
