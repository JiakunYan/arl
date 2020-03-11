#define ARL_DEBUG
#include "arl/arl.hpp"
#include <sstream>
#include <random>

using namespace std;

arl::am_internal::AggBufferSimple* aggBuffer_p;
const int buf_size = 99;

const int MAX_VAL = 100;
const size_t N_STEPS = 50;
std::vector<std::atomic<int>> counts(MAX_VAL);

const bool print_verbose = false;

void worker() {
  using BufPair = pair<char*, int>;
  std::default_random_engine generator(arl::rank_me());
  std::uniform_int_distribution<int> distribution(1, MAX_VAL);

  arl::print("Initializing...\n");

  for (int i = 0; i < N_STEPS; ++i) {
    int val = distribution(generator);
    BufPair result = aggBuffer_p->push(val);
    int count = counts[val]++;

    if (get<1>(result) != 0) {
      char* ptr = get<0>(result);
      int n = get<1>(result) / sizeof(int);
      for (int i = 0; i < n; ++i) {
        int val = *reinterpret_cast<int*>(ptr + i * sizeof(int));
        counts[val]--;
      }
      delete [] ptr;
    }
  }

  arl::barrier();
  arl::print("Finish pushing...\n");

  if (arl::local::rank_me() == 0) {
    BufPair result = aggBuffer_p->flush();

    if (get<1>(result) != 0) {
      char* ptr = get<0>(result);
      int n = get<1>(result) / sizeof(int);
      for (int i = 0; i < n; ++i) {
        int val = *reinterpret_cast<int*>(ptr + i * sizeof(int));
        counts[val]--;
      }
      delete [] ptr;
    }

    bool success = true;
    for (int i = 0; i < MAX_VAL; ++i) {
      int sum = counts[i].load();
      if (sum != 0) {
        printf("Error! Proc %lu, val = %d, sum = %d\n", arl::proc::rank_me(), i, sum);
        success = false;
      }
    }
    if (success) {
      printf("Pass!\n");
    } else {
      exit(1);
    }
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  aggBuffer_p = new arl::am_internal::AggBufferSimple(buf_size);
  for (int i = 0; i < MAX_VAL; ++i) {
    counts[i] = 0;
  }
  arl::run(worker);

  arl::finalize();
}
