#include "arl/arl.hpp"
#include <cassert>
#include <random>
//#include "external/cxxopts.hpp"

using namespace arl;
using namespace std;

const size_t req_payload_size = 64;
struct ReqPayload {
  char data[req_payload_size] = {0};
};

const size_t ack_payload_size = 8;
struct AckPayload {
  char data[ack_payload_size] = {0};
};
ReqPayload req_payload = {0};

AckPayload empty_handler(ReqPayload req_payload) {
  // do nothing
  return AckPayload();
}

void worker() {
  SimpleTimer timer_prepare;
  SimpleTimer timer_push;
  SimpleTimer timer_gex;
  SimpleTimer timer_counter;
  SimpleTimer timer_vec;
  SimpleTimer timer_barrier;

  int num_ops = 1000000;
  size_t my_rank = rank_me();
  size_t nworkers = rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  vector<Future<AckPayload>> futures;
  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; ++i) {
    int remote_worker = distribution(generator);
    {
      using Payload = std::tuple<ReqPayload>;
      using amagg_internal::AmaggTypeWrapper;
      using amagg_internal::AmaggReqMeta;

      ARL_Assert(remote_worker < rank_n(), "");

      timer_prepare.start();
      rank_t remote_proc = remote_worker / local::rank_n();
      int remote_worker_local = remote_worker % local::rank_n();

      Future<AckPayload> my_future(true);
      intptr_t fn_p = am_internal::get_pi_fnptr(&empty_handler);
      intptr_t wrapper_p = am_internal::get_pi_fnptr(&AmaggTypeWrapper<decltype(empty_handler), ReqPayload>::invoker);
      AmaggReqMeta meta{fn_p, wrapper_p, my_future.get_p(), remote_worker_local};
      Payload payload(req_payload);
      timer_prepare.end();

      timer_push.start();
      std::pair<char*, int64_t> result = amagg_internal::amagg_agg_buffer_p[remote_proc].push(meta, payload);
      timer_push.end();

      timer_gex.start();
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          gex_AM_RequestMedium0(backend::tm, remote_proc, amagg_internal::hidx_generic_amagg_reqhandler,
                                std::get<0>(result), std::get<1>(result), GEX_EVENT_NOW, 0);
        }
        delete [] std::get<0>(result);
      }
      timer_gex.end();

      timer_counter.start();
      ++amagg_internal::amagg_req_local_counters[local::rank_me()].val;
      timer_counter.end();

      timer_vec.start();
      futures.push_back(move(my_future));
      timer_vec.end();
    }
  }

  timer_barrier.start();
  barrier();
  for (int i = 0; i < num_ops; ++i) {
    futures[i].get();
  }
  timer_barrier.end();

  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_ff overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth (req/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggReqMeta) + sizeof(ReqPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (req/pure): %.2lf MB/s\n", ((sizeof(ReqPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/gross): %.2lf MB/s\n", ((sizeof(amagg_internal::AmaggAckMeta) + sizeof(AckPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  print("Total single-direction node bandwidth (ack/pure): %.2lf MB/s\n", ((sizeof(AckPayload)) * num_ops * local::rank_n() * 2 / duration_total));
  if (rank_me() == 0) {
    timer_prepare.print_us("prepare");
    timer_push.print_us("push");
    timer_gex.print_us("gex send");
    timer_counter.print_us("counter");
    timer_vec.print_us("vec");
    timer_barrier.print_s("barrier");
//    arl::timer_req.print_us("req handler");
//    arl::timer_ack.print_us("ack handler");
  }
}

int main(int argc, char** argv) {
  // one process per node
  init(15, 16);

  run(worker);

  finalize();
}
