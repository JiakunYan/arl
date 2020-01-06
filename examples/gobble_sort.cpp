#define ARL_DEBUG
#include "arl/arl.hpp"
#include <cassert>
#include "external/cxxopts.hpp"
#include <random>

struct ThreadObjects {
  std::vector<std::atomic<int>> v;
};

arl::WorkerObject<ThreadObjects> mObjects;

// Number of integers to be sorted per node
constexpr size_t n_vals = (1 << 24) * 16;

// Generate random values between [0, 256M)
constexpr size_t range = 1 << 28;
size_t partition_size;

void histogram_handler(int val) {
  size_t bucket_start = arl::rank_me() * partition_size;
  mObjects.get().v[val - bucket_start] += 1;
}

void worker() {
  size_t n_ops = n_vals / arl::local::rank_n();
  arl::print("Sorting %lu integers among %lu workers with aggregation size %lu\n",
          n_vals * arl::proc::rank_n(), arl::rank_n(), arl::get_agg_size());

  // Set up the random engine and generate values
  std::default_random_engine generator(arl::rank_n());
  std::uniform_int_distribution<int> distribution(1, range);

  std::vector<int> vals;

  for (int i = 0; i < n_ops; i++) {
    vals.push_back(distribution(generator) % range);
  }

  // Initialize the buckets
  mObjects.get().v = std::vector<std::atomic<int>>(partition_size);

  std::vector<arl::Future<void> > futures;

  arl::barrier();
  arl::tick_t start = arl::ticks_now();

  for (auto& val : vals) {
    size_t target_rank = val / partition_size;

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
    agg_size = 102;
  }
  ARL_Assert(agg_size >= 0, "");

  arl::init(15, 16);

  mObjects.init();
  arl::set_agg_size(agg_size);

  partition_size = (range + arl::rank_n() - 1) / arl::rank_n();


  arl::run(worker);

  arl::finalize();
}
