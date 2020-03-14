#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"


using namespace arl;

const size_t payload_size = 16;
struct Payload {
  char data[payload_size] = {0};
};

void worker() {

  int num_ops = 100000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, proc::rank_n()-1);

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    size_t target_rank = distribution(generator);
    am_internal::AmffReqMeta meta{1, 1, 1};
    Payload payload{0};
    pair<char*, int> result = am_internal::amff_agg_buffer_p[target_rank].push(meta, payload);
    delete [] get<0>(result);
  }

//  arl::threadBarrier.wait();
  for (int i = 0; i < proc::rank_n(); ++i) {
    vector<pair<char*, int>> results = am_internal::amff_agg_buffer_p[i].flush();
    for (auto result: results) {
//      printf("rank %ld delete [] %p\n", rank_me(), get<0>(result));
      delete [] get<0>(result);
    }
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("AggBuffer overhead is %.2lf us\n", duration_total / num_ops);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
