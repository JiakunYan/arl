#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
const double ONE_MB = 1e6;

template<int N>
struct Payload {
  char data[N] = {0};
};

template<int N>
void empty_handler(Payload<N> payload) {
  // do nothing
}

template<int N>
void worker(int64_t total_MB_to_send) {
  Payload<N> payload;

  double my_byte_to_send = total_MB_to_send * ONE_MB / rank_n();
  int num_ops = my_byte_to_send / N;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    int target_rank = distribution(generator);
    rpc_ff(target_rank, empty_handler<N>, payload);
  }

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("Total MB to send is %d MB\n", total_MB_to_send);
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(Payload<N>)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker<64>, 10);
  run(worker<64>, 100);
  run(worker<64>, 1000);
  run(worker<64>, 10000);
  run(worker<64>, 100000);

  finalize();
}
