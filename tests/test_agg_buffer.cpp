#define ARL_DEBUG
#include "arl/arl.hpp"
#include <sstream>
#include <random>

arl::AggBuffer aggBuffer;
size_t buf_size = 100;

const int MAX_VAL = 100;
const size_t N_STEPS = 50;
std::vector<std::atomic<int>> counts(MAX_VAL);

const bool print_verbose = false;

void worker() {
  using status_t = arl::AggBuffer::status_t;
  std::default_random_engine generator(arl::rank_me());
  std::uniform_int_distribution<int> distribution(1, MAX_VAL);

  arl::print("Initializing...\n");

  for (int i = 0; i < N_STEPS; ++i) {
    int val = distribution(generator);
    status_t status = aggBuffer.push(val);
    while (status == status_t::FAIL) {
      status = aggBuffer.push(val);
    }
    // successful push
    int count = counts[val]++;
    if (print_verbose) {
      printf("Rank %lu push val: (%d, %d), buf_size = %lu\n",
             arl::rank_me(), val, count, aggBuffer.size());
    }
    fflush(stdout);
    if (status == status_t::SUCCESS_AND_FULL) {
      fflush(stdout);
      std::unique_ptr<char[]> buf;
      size_t len = aggBuffer.pop_all(buf) / sizeof(int);
      int* p = (int*) buf.release();
      std::unique_ptr<int[]> buf_int(p);
      if (!print_verbose)
        for (int i = 0; i < len; ++i) {
          counts[buf_int[i]]--;
        }
      else {
        std::ostringstream ostr;
        ostr << "Rank " << arl::rank_me() << " pop vec: ";
        for (int i = 0; i < len; ++i) {
          int count = --counts[buf_int[i]];
          ostr << "(" << val << ", " << count << "), ";
        }
        ostr << "buf_size = " << aggBuffer.size();
        std::cout << ostr.str() << std::endl;
      }
    }
  }

  arl::barrier();
  arl::print("Finish pushing...\n");

  if (arl::local::rank_me() == 0) {
    std::unique_ptr<char[]> buf;
    size_t len = aggBuffer.pop_all(buf) / sizeof(int);
    int* p = (int*) buf.release();
    std::unique_ptr<int[]> buf_int(p);
    if (!print_verbose) {
      for (int i = 0; i < len; ++i) {
        counts[buf_int[i]]--;
      }
    } else {
      std::ostringstream ostr;
      ostr << "Rank " << arl::rank_me() << " pop vec: ";
      for (int i = 0; i < len; ++i) {
        int val = buf_int[i];
        int count = --counts[val];
        ostr << "(" << val << ", " << count << "), ";
      }
      ostr << "buf_size = " << aggBuffer.size();
      std::cout << ostr.str() << std::endl;
    }

    bool success = true;
    for (int i = 0; i < MAX_VAL; ++i) {
      int sum = counts[i].load();
      if (print_verbose && sum != 0) {
        printf("Error! Proc %lu, val = %d, sum = %d\n", arl::proc::rank_me(), i, sum);
        success = false;
      } else {
        assert(sum == 0);
      }
    }
    if (print_verbose && success) {
      printf("Pass!\n");
    } else {
      printf("Pass!\n");
    }
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  aggBuffer.init<int>(buf_size);
  for (int i = 0; i < MAX_VAL; ++i) {
    counts[i] = 0;
  }
  arl::run(worker);

  arl::finalize();
}
