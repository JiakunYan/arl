#include "arl.hpp"
#include <cassert>
#include <random>
#include "external/cxxopts.hpp"

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
  register_amaggrd(usleep_handler, sleep_us, req_payload);

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    auto future = rpc_aggrd(target_rank, usleep_handler, sleep_us, req_payload);
    futures.push_back(move(future));
  }

  barrier();
  for (int i = 0; i < num_ops; ++i) {
    futures[i].get();
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  using amaggrd_internal::AmaggrdReqMeta;
  using amaggrd_internal::AmaggrdReqPayload;
  using amaggrd_internal::AmaggrdAckMeta;
  using amaggrd_internal::AmaggrdAckPayload;
  print("sleep time is %ld us\n", sleep_us);
  print("rpc_aggrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
}

int main(int argc, char** argv) {
  cxxopts::Options options("ARL Benchmark", "Benchmark of ARL system");
  options.add_options()
      ("req", "Size of Request Payload", cxxopts::value<int>()->default_value("64"))
      ("ack", "Size of Request Payload", cxxopts::value<int>()->default_value("64"))
      ;
  auto result = options.parse(argc, argv);

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
