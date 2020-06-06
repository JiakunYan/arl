//#define ARL_DEBUG
#include "arl/arl.hpp"
#include <cassert>
#include <random>
#include <unistd.h>
#include "external/typename.hpp"
//#include "external/cxxopts.hpp"

using namespace arl;

template<int N>
struct Payload {
  char data[N] = {0};
};

void usleep_handler(int64_t sleep_time, Payload<56> payload) {
  // do nothing
  usleep(sleep_time);
}

void worker(int64_t sleep_us) {
  Payload<56> payload;
  int num_ops;
  if (sleep_us > 0)
    num_ops = 1000000 / sleep_us;
  else
    num_ops = 1000000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);

  barrier();
  tick_t start = ticks_now();
  register_amffrd(usleep_handler, int64_t(), payload);

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    rpc_ffrd(target_rank, usleep_handler, sleep_us, payload);
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  using amffrd_internal::AmffrdReqPayload;
  print("sleep time is %ld us\n", sleep_us);
  print("rpc_ffrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker, 0);
  run(worker, 1);
  run(worker, 10);
  run(worker, 30);
  run(worker, 100);
  run(worker, 300);
  run(worker, 1000);

  finalize();
}
