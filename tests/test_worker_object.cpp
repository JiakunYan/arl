#define ARL_DEBUG
  #include "arl.hpp"
  #include <cassert>

struct ThreadObjects {
    std::vector<std::atomic<int>> v;
};

arl::WorkerObject<ThreadObjects> mObjects;

void histogram_handler(int idx) {
  mObjects.get().v[idx] += 1;
}

void worker() {

  int local_range = 5;
  int total_range = local_range * (int) arl::rank_n();

  size_t my_rank = arl::local::rank_me();
  int nworkers = (int) arl::rank_n();

  mObjects.get().v = std::vector<std::atomic<int>>(local_range);

  using rv = decltype(arl::rpc(size_t(), histogram_handler, int()));
  std::vector<rv> futures;

  arl::barrier();
  for (int i = 0 ; i < total_range; i++) {
    int idx = (i + local_range * (int)my_rank) % total_range;
    size_t target_rank = idx / local_range;
    int val = idx % local_range;
    auto f = arl::rpc(target_rank, histogram_handler, val);
    futures.push_back(std::move(f));
  }

  arl::barrier();

  for (int i = 0 ; i < total_range; i++) {
    futures[i].get();
  }

  arl::barrier();

  for (int i = 0; i < local_range; i++) {
    assert(mObjects.get().v[i] == nworkers);
  }
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16);
  mObjects.init();

  arl::run(worker);

  arl::finalize();
}
