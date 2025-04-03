//#define ARL_DEBUG
#include "arl.hpp"
#include <cassert>
#include <random>
#include "external/typename.hpp"
//#include "external/cxxopts.hpp"

using namespace arl;

template<int N>
struct Payload {
  char data[N] = {0};
};

template<int N>
void empty_handler(Payload<N> payload) {
  // do nothing
}

template<int N>
void worker() {
  Payload<N> payload = {0};

  int num_ops = 1000000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);

  barrier();
  tick_t start = ticks_now();
  register_amffrd(empty_handler<N>, payload);

  for (int i = 0; i < num_ops; i++) {
    int target_rank = distribution(generator);
    rpc_ffrd(target_rank, empty_handler<N>, payload);
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("req payload size is %d Byte\n", N);
  print("rpc_ffrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (req/gross): %.2lf MB/s\n", ((sizeof(Payload<N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(Payload<N>)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker<8>);
  run(worker<16>);
  run(worker<32>);
  run(worker<48>);
  run(worker<64>);
  run(worker<128>);

  finalize();
}
