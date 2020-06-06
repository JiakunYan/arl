#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
using namespace std;

template<int N>
struct ReqPayload {
  char data[N] = {0};
};

template<int N>
struct AckPayload {
  char data[N] = {0};
};

void usleep_handler(int64_t sleep_time, ReqPayload<56> payload) {
  // do nothing
  usleep(sleep_time);
}

void worker(int64_t sleep_us) {
  ReqPayload<56> req_payload{};

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
  vector<Future<void>> futures;
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    auto future = rpc_agg(target_rank, usleep_handler, sleep_us, req_payload);
    futures.push_back(move(future));
  }

  barrier();
  for (int i = 0; i < num_ops; ++i) {
    futures[i].get();
//    int result = futures[i].get();
//    if (result != 233) {
//      printf("error!");
//    }
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("sleep time is %ld us\n", sleep_us);
  print("rpc_agg overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
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
