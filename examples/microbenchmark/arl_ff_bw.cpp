#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;

void empty_handler() {
  // do nothing
}

void worker() {

  int num_ops = 100000;
  int total_num_ops = num_ops * (int) rank_n();
  double ticks_step = 0;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    size_t target_rank = distribution(generator);
    rpc_ff(target_rank, empty_handler);
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_ff overhead is %.2lf us\n", duration_total / num_ops);
  print("Total single-direction node bandwidth: %lu MB/s\n", (unsigned long) (sizeof(am_internal::AmffReqMeta) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
