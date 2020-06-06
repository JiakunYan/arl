//#define ARL_DEBUG
#include "arl/arl.hpp"
#include <cassert>
#include <random>
#include "external/typename.hpp"
//#include "external/cxxopts.hpp"

using namespace arl;

const size_t payload_size = 32;
struct ReqPayload {
  char data[payload_size] = {0};
};
ReqPayload my_payload = {0};

void empty_handler(ReqPayload payload) {
  // do nothing
}

__thread SimpleTimer timer_lpc;
__thread SimpleTimer timer_push;
__thread SimpleTimer timer_gex;
__thread SimpleTimer timer_counter;
__thread SimpleTimer timer_barrier;

template <typename Fn, typename... Args>
void rpc_ffrd_profile(rank_t remote_worker, Fn&& fn, Args&&... args) {
  using Payload = amffrd_internal::AmffrdReqPayload<std::remove_reference_t<Args>...>;
  using amffrd_internal::AmffrdTypeWrapper;
  using amffrd_internal::AmffrdReqMeta;

  ARL_Assert(amffrd_internal::global_meta_p != nullptr, "call rpc_ffrd before register_ffrd!");
  ARL_Assert(remote_worker < rank_n(), "");

  rank_t remote_proc = remote_worker / local::rank_n();
  int remote_worker_local = remote_worker % local::rank_n();
//  if (remote_proc == proc::rank_me()) {
//     local precedure call
//    timer_lpc.start();
//    amff_internal::run_lpc(remote_worker_local, std::forward<Fn>(fn), std::forward<Args>(args)...);
//    timer_lpc.end();
//    return;
//  }

  Payload payload{remote_worker_local, std::make_tuple(std::forward<Args>(args)...)};

  timer_push.start();
  std::pair<char*, int> result = amffrd_internal::amffrd_agg_buffer_p[remote_proc].push(std::move(payload));
  timer_push.end();

  timer_gex.start();
  if (std::get<0>(result) != nullptr) {
    if (std::get<1>(result) != 0) {
      amffrd_internal::send_amffrd_to_gex(remote_proc, *amffrd_internal::global_meta_p,
                                          std::get<0>(result), std::get<1>(result));
      arl::progress_external();
    }
    delete [] std::get<0>(result);
  }
  timer_gex.end();

  timer_counter.start();
  ++(amffrd_internal::amffrd_req_counters[remote_proc].val);
  timer_counter.end();
}

void worker() {
  int num_ops = 1000000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);

  barrier();
  tick_t start = ticks_now();
  register_amffrd(empty_handler, my_payload);

  for (int i = 0; i < num_ops; ++i) {
    int remote_worker = distribution(generator);
    rpc_ffrd_profile(remote_worker, empty_handler, my_payload);
  }

  timer_barrier.start();
  barrier();
  timer_barrier.end();

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_ffrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (pure): %.2lf MB/s\n", ((sizeof(ReqPayload)) * num_ops * local::rank_n() * 2 / duration_total));
//  if (rank_me() == 5) {
  timer_lpc.col_print_us("lpc");
  timer_push.col_print_us("push");
  timer_gex.col_print_us("gex send");
  timer_counter.col_print_us("counter");
  timer_barrier.col_print_s("barrier");
  if (local::rank_me() == 0) {
    printf("process %ld recv %ld rpc\n", proc::rank_me(), amffrd_internal::amffrd_recv_counter->val.load());
  }
//  }
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
  return 0;
}
