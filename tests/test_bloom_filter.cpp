//
// Created by Jiakun Yan on 1/12/20.
//

#define ARL_DEBUG
#include "arl/arl.hpp"

using namespace arl;
using namespace std;

struct flat_string_t {
  char str[10] = "";
  flat_string_t(string s) {
    if (s.length() >= 10) {
      ARL_Error("Overload!");
    }
    strcpy(str, s.c_str());
  }
};

namespace std {
  template <>
  struct hash<flat_string_t> {
    uint64_t operator()(flat_string_t val) {
      return std::hash<std::string>{}(string(val.str));
    }
  };
}

size_t test_num = 10000;
double p = 0.01;
local::BloomFilter<flat_string_t> bloomFilter(test_num, p);

void worker(size_t local_num) {
  for (size_t i = 0; i < local_num; ++i) {
    flat_string_t val = string_format(rank_me(), "_", i);
    bloomFilter.add(val);
  }

  barrier();

  size_t fn_error = 0;
  for (size_t i = 0; i < local_num; ++i) {
    flat_string_t val = string_format(rank_me(), "_", i);
    bool noerror = bloomFilter.possibly_contains(val);
    if (!noerror) {
      ++fn_error;
    }
  }

  size_t error_num = 0;
  for (size_t i = local_num; i < local_num + test_num; ++i) {
    flat_string_t val = string_format(rank_me(), "_", i);
    bool error = bloomFilter.possibly_contains(val);
    if (error) {
      error_num++;
    }
  }

  barrier();
  size_t total_fn_error = reduce_one(fn_error, op_plus(), 0);
  size_t total_error_num = reduce_one(error_num, op_plus(), 0);
  double error_rate = (double)total_error_num / test_num / rank_n();
  print("False negative number is %lu\n", total_fn_error);
  print("Total entry number is %lu\n", test_num);
  print("Design error rate is %lf\n", p);
  print("Hash function number is %lu\n", bloomFilter.hash_fn_n());
  print("Data size is %lu Bytes\n", test_num * sizeof(flat_string_t));
  print("Actual memory usage is %lu Bytes\n", bloomFilter.memory_usage());
  print("Actual error rate is %lf\n", error_rate);
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  arl::run(worker, test_num / 15);
  arl::finalize();
  return 0;
}