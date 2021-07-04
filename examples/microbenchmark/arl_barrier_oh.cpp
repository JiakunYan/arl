#include "arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"
using namespace arl;

void worker() {

  int num_ops = 1000;

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    barrier();
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("AggBuffer average overhead is %.2lf us\n", duration_total / num_ops);
  print("AggBuffer total throughput is %.2lf M/s\n", (double) local::rank_n() * num_ops / duration_total);

}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
