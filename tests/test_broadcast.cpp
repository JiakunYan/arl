#define ARL_DEBUG
#include "arl/arl.hpp"
#include <vector>

void worker() {
  std::vector<size_t> v(arl::nworkers(), -1);
  v[arl::my_worker()] = arl::my_worker();
  for (int i = 0; i < arl::nworkers(); ++i) {
    arl::broadcast(v[i], i);
    assert(v[i] == i);
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  arl::run(worker);

  arl::finalize();
}
