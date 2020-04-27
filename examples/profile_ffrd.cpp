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

void worker() {
  int local_req_counter = 0;
  SimpleTimer timer_prepare;
  SimpleTimer timer_push;
  SimpleTimer timer_gex;
  SimpleTimer timer_counter;
  SimpleTimer timer_barrier;

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

    {
      using Payload = amffrd_internal::AmffrdReqPayload<ReqPayload>;
      using amffrd_internal::AmffrdTypeWrapper;
      using amffrd_internal::AmffrdReqMeta;

      ARL_Assert(amffrd_internal::global_meta_p != nullptr, "call rpc_ffrd before register_ffrd!");
      ARL_Assert(remote_worker < rank_n(), "");

      timer_prepare.start();
      rank_t remote_proc = remote_worker / local::rank_n();
      int remote_worker_local = remote_worker % local::rank_n();
      Payload payload{remote_worker_local, std::make_tuple(my_payload)};
      timer_prepare.end();

      timer_push.start();
      std::pair<char*, int> result = amffrd_internal::amffrd_agg_buffer_p[remote_proc].push(payload);
      timer_push.end();

      timer_gex.start();
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          amffrd_internal::send_amffrd_to_gex(remote_proc, *amffrd_internal::global_meta_p,
                                              std::get<0>(result), std::get<1>(result));
        }
        delete [] std::get<0>(result);
      }
      timer_gex.end();

      timer_counter.start();
      ++local_req_counter;
      timer_counter.end();
    }
  }

  (*amffrd_internal::amffrd_req_counter) += local_req_counter;
  timer_barrier.start();
  barrier();
  timer_barrier.end();

  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_ffrd overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (pure): %lu MB/s\n", (unsigned long) ((sizeof(ReqPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  if (rank_me() == 0) {
    timer_prepare.print_us("prepare");
    timer_push.print_us("push");
    timer_gex.print_us("gex send");
    timer_counter.print_us("counter");
    timer_barrier.print_s("barrier");
  }
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
  return 0;
}
