#define ARL_DEBUG
#include "arl/arl.hpp"
#include <string>

void worker() {
  size_t num_ops = 5;
  size_t hashmap_size = arl::rank_n() * num_ops * 2;
  arl::HashMap<size_t, size_t> hashmap(hashmap_size);

  for (int i = 0; i < num_ops; ++i) {
    size_t my_val = arl::rank_me();
    size_t my_key = my_val + i * arl::rank_n();
    hashmap.insert(my_key, my_val).get();
  }

  arl::barrier();

  for (int i = 0; i < num_ops; ++i) {
    size_t my_val = (arl::rank_me() + 1) % arl::rank_n();
    size_t my_key = my_val + i * arl::rank_n();
    size_t result = hashmap.find(my_key).get();
    if (result != my_val) {
      printf("Error! key %lu, %lu != %lu\n", my_key, result, my_val);
      abort();
    }
  }
  arl::print("Pass!\n");
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(2, 3);
  arl::run(worker);

  arl::finalize();
}
