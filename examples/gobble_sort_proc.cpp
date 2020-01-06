#define ARL_PROFILE
#include "arl/arl.hpp"
#include <cassert>
#include "external/cxxopts.hpp"
#include <random>

// Generate random values between [0, 256M)
constexpr size_t range = 1 << 28;
size_t partition_size;

std::vector<std::atomic<int>> buckets;

void histogram_handler(int idx) {
  buckets[idx] += 1;
}

void worker() {
  // Sort 16M integers per worker
  constexpr size_t n_vals = 1 << 24;


  arl::print("Sorting %lu integers among %lu workers with aggregation size %lu\n",
          n_vals, arl::rank_n(), arl::get_agg_size());

  // Set up the random engine and generate values
  std::default_random_engine generator(arl::rank_n());
  std::uniform_int_distribution<int> distribution(1, range);

  std::vector<int> vals;

  for (int i = 0; i < n_vals; i++) {
    vals.push_back(distribution(generator) % range);
  }

  std::vector<arl::Future<void> > futures;

  arl::barrier();
  arl::tick_t start = arl::ticks_now();

  for (auto& val : vals) {
    size_t target_rank = (val / partition_size) * arl::local::rank_n();

    auto future = arl::rpc_agg(target_rank, histogram_handler, val);
    futures.push_back(std::move(future));
  }

  arl::barrier();

  for (auto& future: futures) {
    future.get();
  }

  arl::barrier();
  arl::tick_t end = arl::ticks_now();

  double duration = arl::ticks_to_ns(end - start) / 1e9;
  arl::print("%lf seconds\n", duration);
}

int main(int argc, char** argv) {
  // one process per node
  size_t agg_size = 0;
  cxxopts::Options options("ARL Benchmark", "Benchmark of ARL system");
  options.add_options()
      ("size", "Aggregation size", cxxopts::value<size_t>())
      ;
  auto result = options.parse(argc, argv);
  try {
    agg_size = result["size"].as<size_t>();
  } catch (...) {
    agg_size = 0;
  }
  ARL_Assert(agg_size >= 0, "");

  arl::init(15, 16);
  arl::set_agg_size(agg_size);

  // Initialize the buckets
  partition_size = (range + arl::proc::rank_n() - 1) / arl::proc::rank_n();
  buckets = std::vector<std::atomic<int>>(partition_size);

  arl::run(worker);

  arl::finalize();
}
