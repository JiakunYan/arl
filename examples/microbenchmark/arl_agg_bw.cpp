#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
using namespace std;

const size_t payload_size = 64;
struct Payload {
  char data[payload_size] = {0};
};

const size_t ack_payload_size = 8;
struct AckPayload {
  char data[ack_payload_size] = {0};
};
Payload payload = {0};

AckPayload empty_handler(Payload payload) {
  // do nothing
  return AckPayload();
}

void worker() {

  int num_ops = 1000000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  vector<Future<AckPayload>> futures;
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; ++i) {
    int target_rank = distribution(generator);
    auto future = rpc_agg(target_rank, empty_handler, payload);
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
  print("rpc_ff overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (req/gross): %lu MB/s\n", (unsigned long) ((sizeof(amagg_internal::AmaggReqMeta) + sizeof(Payload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (req/pure): %lu MB/s\n", (unsigned long) ((sizeof(Payload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/gross): %lu MB/s\n", (unsigned long) ((sizeof(amagg_internal::AmaggAckMeta) + sizeof(AckPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/pure): %lu MB/s\n", (unsigned long) ((sizeof(AckPayload)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
