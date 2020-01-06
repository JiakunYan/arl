#define ARL_DEBUG
#include "arl/arl.hpp"
#include <vector>

using namespace arl;

void worker() {
  int result0 = local::reduce_all(local::rank_me(), op_plus());
  assert(result0 == 105);

  int answer = (rank_n() - 1) * rank_n() / 2;

  int result1 = reduce_one(rank_me(), op_plus(), 1);
  if (rank_me() == 1) {
    ARL_Assert(result1 == answer, "");
    assert(result1 == answer);
  }

  int result2 = reduce_all(rank_me(), op_plus());
  assert(result2 == answer);
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);
  run(worker);

  finalize();
}
