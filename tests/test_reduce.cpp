#define ARL_DEBUG
#include "arl.hpp"
#include <vector>
#include <cassert>

using namespace arl;

void worker() {
  // test reduce of value version
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

  // test reduce of vector version
  std::vector<int> question{0, 1, 2, 3};
  auto result3 = reduce_all(question, op_plus());
  for (int i = 0; i < question.size(); ++i) {
    ARL_Assert(result3[i] == question[i] * rank_n());
  }

  auto result4 = reduce_one(question, op_plus(), 0);
  if (rank_me() == 0)
    for (int i = 0; i < question.size(); ++i)
      ARL_Assert(result4[i] == question[i] * rank_n());
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);
  run(worker);

  finalize();
}
