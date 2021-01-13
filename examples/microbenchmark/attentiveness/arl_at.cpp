#include "arl.hpp"
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

template<int REQ_N, int ACK_N>
AckPayload<ACK_N> empty_handler(ReqPayload<REQ_N> payload) {
  // do nothing
  return AckPayload<ACK_N>();
}

template<int REQ_N, int ACK_N>
void worker(int compute_us) {
  ReqPayload<REQ_N> req_payload;

  int num_ops = 10000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  vector<Future<AckPayload<ACK_N>>> futures;
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    arl::usleep(compute_us);
    auto future = rpc(target_rank, empty_handler<REQ_N, ACK_N>, req_payload);
    future.get();
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("compute time is %d us\n", compute_us);
  print("rpc latency is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker<64, 8>, 0);
  run(worker<64, 8>, 10);
  run(worker<64, 8>, 32);
  run(worker<64, 8>, 100);
  run(worker<64, 8>, 320);
  run(worker<64, 8>, 1000);

  finalize();
}
