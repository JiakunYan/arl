#define ARL_DEBUG
#include "arl.hpp"
#include <sstream>
#include <random>
#include "external/typename.hpp"

using namespace std;

using AggBuffer = arl::am_internal::AggBufferAtomic;
//using AggBuffer = arl::am_internal::AggBufferSimple;
//using AggBuffer = arl::am_internal::AggBufferAdvanced;
AggBuffer* aggBuffer_p;
const int buf_size = 103;

const int MAX_VAL = 100;
const size_t N_STEPS = 500;
std::vector<std::atomic<int>> counts(MAX_VAL);

const bool print_verbose = false;

void worker() {
  using BufPair = std::pair<char*, int64_t>;
  std::default_random_engine generator(arl::rank_me());
  std::uniform_int_distribution<int> distribution(1, MAX_VAL);

  arl::print("Initializing...\n");

  for (int i = 0; i < N_STEPS; ++i) {
    int val = distribution(generator);
    BufPair result = aggBuffer_p->push(val);
    int count = counts[val]++;

    if (get<0>(result) != nullptr) {
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
  std::vector<BufPair> results = aggBuffer_p->flush();

  for (auto result: results) {
    if (get<0>(result) != nullptr) {
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
  if (arl::local::rank_me() == 0) {
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
  cout << "Testing " << type_name<AggBuffer>() << endl;
  aggBuffer_p = new AggBuffer();
  aggBuffer_p->init(buf_size);
  for (int i = 0; i < MAX_VAL; ++i) {
    counts[i] = 0;
  }
  arl::run(worker);

  arl::finalize();
}
