#include "arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"


using namespace arl;

const size_t payload_size = 32;
struct Payload {
  char data[payload_size];
};
using arl::am_internal::AggBuffer;
//using AggBuffer = arl::am_internal::AggBufferAtomic; // 0.18/118, 0.18/70
//using AggBuffer = arl::am_internal::AggBufferSimple; // (0.42/123) 0.32/95 0.26/87
//using AggBuffer = arl::am_internal::AggBufferLocal; // (0.15/161) 0.11/162
//using AggBuffer = arl::am_internal::AggBufferAdvanced; // (0.25/853) 0.16/700

AggBuffer** buffer_p;
const int buffer_cap = 1023 * 64;
int buf_n = 32;

void init_buf() {
  buffer_p = new AggBuffer*[buf_n];
  for (int i = 0; i < buf_n; ++i) {
    if (config::aggBufferType == config::AGG_BUFFER_SIMPLE)
      buffer_p[i] = new arl::am_internal::AggBufferSimple();
    else if (config::aggBufferType == config::AGG_BUFFER_LOCAL)
      buffer_p[i] = new am_internal::AggBufferLocal();
    else {
      assert(config::aggBufferType == config::AGG_BUFFER_ATOMIC);
      buffer_p[i] = new am_internal::AggBufferAtomic();
    }
    buffer_p[i]->init(buffer_cap, 0);
  }
}

void exit_buf() {
  for (int i = 0; i < buf_n; ++i) {
    delete buffer_p[i];
  }
  delete [] buffer_p;
}

void worker() {

  int num_ops = 1000000;
  size_t my_rank = rank_me();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, buf_n-1);

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    size_t target_rank = distribution(generator);
    Payload payload{0};
    std::pair<char*, int64_t> result = buffer_p[target_rank]->push((char*) (char*) &payload, sizeof(payload));
    delete []  std::get<0>(result);
  }

  barrier();
  tick_t start_flush = ticks_now();
  for (int i = 0; i < buf_n; ++i) {
    std::vector<std::pair<char*, int64_t>> results = buffer_p[i]->flush();
    for (auto result: results) {
      delete []  std::get<0>(result);
    }
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_flush = ticks_to_us(end_wait - start_flush);
  double duration_total = ticks_to_us(end_wait - start);
  print("AggBuffer flush overhead is %.2lf us\n", duration_flush);
  print("AggBuffer average overhead is %.2lf us\n", duration_total / num_ops);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);
  init_buf();

  run(worker);

  exit_buf();
  finalize();
}
