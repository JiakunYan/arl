#include "arl/arl.hpp"

using namespace arl;

rank_t say_hello(rank_t source) {
  printf("Worker %lu/%lu is running an RPC from worker %lu!\n", my_worker(), nworkers(), source);
  return my_worker();
}

void worker(int arg) {
  rank_t target = (my_worker() + 1) % nworkers();
  auto future = rpc(target, say_hello, my_worker());
  rank_t result = future.get();
  printf("Worker %lu/%lu's RPC has been executed by worker %lu!\n", my_worker(), nworkers(), result);
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16); // 15 worker, 1 progress thread per process
  int arg = 132; // you can pass some arguments if you want
  arl::run(worker, arg);

  arl::finalize();
}
