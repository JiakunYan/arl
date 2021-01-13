#define ARL_DEBUG
#include "arl.hpp"

using T = std::vector<int>;
const int VEC_LEN = 10;

int fn(T *vec, int index) {
  return vec->at(index);
}

void worker() {
  T vec(VEC_LEN, arl::rank_me());
  arl::DistWrapper vec_wrap(&vec);

  std::vector<arl::Future<int>> futs;
  for (int i = 0; i < arl::rank_n(); ++i) {
    arl::rank_t remote_target = (arl::rank_me() + i + 1) % arl::rank_n();
    for (int j = 0; j < VEC_LEN; ++j) {
//      using RPC_Fn = decltype(arl::rpc_agg<arl::rank_t, decltype(fn), T*, int>);
//      vec_wrap.remote_invoke<RPC_Fn, arl::rank_t, decltype(fn), int>(arl::rpc_agg, remote_target, fn, j);
      auto fut = arl::rpc_agg(remote_target, fn, vec_wrap[remote_target], j);
      futs.push_back(std::move(fut));
    }
  }

  arl::barrier();
  int k = 0;
  for (int i = 0; i < arl::rank_n(); ++i) {
    arl::rank_t remote_target = (arl::rank_me() + i + 1) % arl::rank_n();
    for (int j = 0; j < VEC_LEN; ++j) {
      arl::ARL_Assert(futs[k].get() == remote_target, futs[k].get(), "!=", remote_target);
      ++k;
    }
  }
  arl::barrier();
  arl::print("Pass!\n");
}

int main(int argc, char** argv) {
  // one process per node
  arl::init();
  arl::run(worker);

  arl::finalize();
}
