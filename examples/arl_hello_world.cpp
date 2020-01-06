#include "arl/arl.hpp"

using namespace arl;

rank_t say_hello(rank_t source) {
  printf("Worker %lu/%lu is running an RPC from worker %lu!\n", rank_me(), rank_n(), source);
  return rank_me();
}

void worker(int arg) {
  rank_t target = (rank_me() + 1) % rank_n();
  auto future = rpc(target, say_hello, rank_me());
  rank_t result = future.get();
  printf("Worker %lu/%lu's RPC has been executed by worker %lu!\n", rank_me(), rank_n(), result);
}

int main(int argc, char** argv) {
  // one process per node
  arl::init(15, 16); // 15 worker, 1 progress thread per process
  int arg = 132; // you can pass some arguments if you want
  arl::run(worker, arg);

  arl::finalize();
}
