#include "arl/arl.hpp"
#include <cassert>
#include "external/cxxopts.hpp"
#include "external/bcl/containers/experimental/SafeChecksumQueue.hpp"

const int num_ops = 100000;
const int queue_size = 1000;
const char* title = "broadcast";
BCL::ChecksumQueue<int> queue(0, queue_size);

void do_something(int i) {
  queue.push(i);
}

void worker() {
  int total_num_ops = num_ops * (int) arl::rank_n();

  arl::print("Start benchmark %s on %lu ops\n", title, num_ops);
  arl::barrier();
  arl::tick_t start = arl::ticks_now();

  for (int i = 0; i < num_ops; i++) {
    do_something(i);
  }

  arl::barrier();
  arl::tick_t end = arl::ticks_now();

  double duration = arl::ticks_to_ns(end - start) / 1e3;
  double ave_overhead = duration / num_ops;
  arl::print("%lu ops in %.2lf s\n", duration / 1e6, num_ops);
  arl::print("%.2lf us/op\n", ave_overhead);
}

int main(int argc, char** argv) {
  arl::init(15, 16);

  arl::run(worker);

  arl::finalize();
}
