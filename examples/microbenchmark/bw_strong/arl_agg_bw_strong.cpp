#include "arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
using namespace std;
const double ONE_MB = 1e6;

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
void worker(int64_t total_MB_to_send) {
  ReqPayload<REQ_N> req_payload;

  double my_byte_to_send = total_MB_to_send * ONE_MB / rank_n();
  int num_ops = my_byte_to_send / REQ_N;
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
    auto future = rpc_agg(target_rank, empty_handler<REQ_N, ACK_N>, req_payload);
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
  print("Total MB to send is %d MB\n", total_MB_to_send);
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(ReqPayload<REQ_N>)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker<64, 8>, 10);
  run(worker<64, 8>, 100);
  run(worker<64, 8>, 1000);
  run(worker<64, 8>, 10000);
  run(worker<64, 8>, 100000);

  finalize();
}
