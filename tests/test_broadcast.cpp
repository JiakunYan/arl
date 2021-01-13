#define ARL_DEBUG
#include "arl.hpp"
#include <vector>
#include <cassert>

void worker() {
  std::vector<size_t> v(arl::rank_n(), -1);
  v[arl::rank_me()] = arl::rank_me();
  for (int i = 0; i < arl::rank_n(); ++i) {
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
