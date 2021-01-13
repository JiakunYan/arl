#include "arl.hpp"
#include <queue>
#include <mutex>
#include <thread>
#include <random>

bool thread_run = true;

std::vector<std::atomic<int>> receiveds(16);
size_t req_num;
size_t rep_num;
arl::ThreadBarrier threadBarrier;

void empty_handler(gex_Token_t token, void *buf, size_t nbytes, gex_AM_Arg_t id) {
  {

  }
  gex_AM_ReplyMedium(token, rep_num, nullptr, 0, GEX_EVENT_NOW, 0, id);
}

void reply_handler(gex_Token_t token, void *buf, size_t nbytes, gex_AM_Arg_t id) {
  receiveds[id]++;
}

void worker(int id) {
  int issued = 0;
  size_t num_ops = 10000;
  std::default_random_engine generator(arl::backend::rank_me());
  std::uniform_int_distribution<int> distribution(0, arl::backend::rank_n()-1);
  distribution(generator);

  srand48(arl::backend::rank_me());
  threadBarrier.wait();
  if (id == 0) {
    arl::backend::barrier();
  }
  threadBarrier.wait();
  auto begin = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < num_ops; i++) {
    size_t remote_proc = distribution(generator);
//    size_t remote_proc = lrand48() % (BCL::nprocs() - 1);
//    if (remote_proc >= BCL::rank()) {
//      remote_proc++;
//    }
    int rv = gex_AM_RequestMedium(arl::backend::internal::tm, remote_proc, req_num, nullptr, 0, GEX_EVENT_NOW, 0, id);
    issued++;

    while (receiveds[id] < issued) {
      gasnet_AMPoll();
    }
  }

  threadBarrier.wait();
  if (id == 0) {
    arl::backend::barrier();
  }
  threadBarrier.wait();
  auto end = std::chrono::high_resolution_clock::now();
  double duration_s = std::chrono::duration<double>(end - begin).count();
  if (id == 0) {
    arl::proc::print("AM latency is %.2lf us (total %.2lf s)\n", duration_s * 1e6 / num_ops, duration_s);
  }
}

int main() {
  arl::backend::init(2048);

  size_t max_args = gex_AM_MaxArgs();
  size_t handler_num = GEX_AM_INDEX_BASE;
  req_num = handler_num++;
  rep_num = handler_num++;

  gex_AM_Entry_t entry[2];
  entry[0].gex_index = req_num;
  entry[0].gex_fnptr = (gex_AM_Fn_t) empty_handler;
  entry[0].gex_flags = GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST;
  entry[0].gex_nargs = 1;

  entry[1].gex_index = rep_num;
  entry[1].gex_fnptr = (gex_AM_Fn_t) reply_handler;
  entry[1].gex_flags = GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REPLY;
  entry[1].gex_nargs = 1;

  int rv = gex_EP_RegisterHandlers(arl::backend::internal::ep, entry, 2);

  threadBarrier.init(16);

  std::vector<std::thread> worker_pool;
  for (size_t i = 0; i < 16; ++i) {
    auto t = std::thread(worker, i);
    worker_pool.push_back(std::move(t));
  }

  for (size_t i = 0; i < 16; ++i) {
    worker_pool[i].join();
  }
  arl::backend::finalize();
  return 0;

}