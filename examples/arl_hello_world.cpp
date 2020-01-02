#define ARL_DEBUG
#include "arl/arl.hpp"

void worker(int val) {
  std::printf("Hello, I am thread %lu/%lu! receive %d\n", arl::my_worker(), arl::nworkers(), val);
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  int val = 132;
  arl::run(worker, val);

  arl::finalize();
}
