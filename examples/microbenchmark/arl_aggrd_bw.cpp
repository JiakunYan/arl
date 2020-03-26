#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
using namespace std;

const size_t payload_size = 21;
struct Payload {
  char data[payload_size] = {0};
};

const size_t ack_payload_size = 15;
struct AckPayload {
  char data[ack_payload_size] = {0};
};
Payload payload = {0};

AckPayload empty_handler(Payload payload) {
  // do nothing
  return AckPayload();
}

void worker() {

  int num_ops = 100000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  vector<Future<AckPayload>> futures;
  barrier();
  tick_t start = ticks_now();
  register_amaggrd(empty_handler, payload);

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    auto future = rpc_aggrd(target_rank, empty_handler, payload);
    futures.push_back(move(future));
  }

  barrier();
  for (int i = 0; i < num_ops; ++i) {
    futures[i].get();
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_aggrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth: %lu MB/s\n", (unsigned long) ((sizeof(amagg_internal::AmaggReqMeta) + sizeof(Payload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (pure): %lu MB/s\n", (unsigned long) ((sizeof(Payload)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
