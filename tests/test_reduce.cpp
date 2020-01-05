#define ARL_DEBUG
#include "arl/arl.hpp"
#include <vector>

using namespace arl;

void worker() {
  int result0 = local::reduce_all(my_worker_local(), op_plus());
  assert(result0 == 105);

  int answer = (nworkers() - 1) * nworkers() / 2;

  int result1 = reduce_one(my_worker(), op_plus(), 1);
  if (my_worker() == 1) {
    ARL_Assert(result1 == answer, "");
    assert(result1 == answer);
  }

  int result2 = reduce_all(my_worker(), op_plus());
  assert(result2 == answer);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);
  run(worker);

  finalize();
}
