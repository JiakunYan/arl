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

template<int REQ_N, int ACK_N>
AckPayload<ACK_N> empty_handler(ReqPayload<REQ_N> payload) {
  // do nothing
  return AckPayload<ACK_N>();
}

template<int REQ_N, int ACK_N>
void worker() {
  ReqPayload<REQ_N> req_payload;

  int num_ops = 1000000;
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
  print("req payload size is %d Byte\n", REQ_N);
  print("ack payload size is %d Byte\n", ACK_N);
  print("rpc_agg overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (req/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggReqMeta) + sizeof(ReqPayload<REQ_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(ReqPayload<REQ_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggAckMeta) + sizeof(AckPayload<ACK_N>)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/pure): %.2lf MB/s\n", ((sizeof(AckPayload<ACK_N>)) * num_ops * local::rank_n() * 2 / duration_total));
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker<8, 8>);
  run(worker<16, 8>);
  run(worker<32, 8>);
  run(worker<48, 8>);
  run(worker<64, 8>);
  run(worker<128, 8>);

  finalize();
}
