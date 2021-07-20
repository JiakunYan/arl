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

template<int N>
struct Payload {
  char data[N];
};

struct SleepPayload {
  int64_t sleep_time;
  Payload<56> useless;
};

void empty_handler(gex_Token_t token, void *buf, size_t nbytes, gex_AM_Arg_t id) {
  {
    SleepPayload *payload = (SleepPayload*) buf;
    usleep(payload->sleep_time);
  }
  gex_AM_ReplyMedium(token, rep_num, nullptr, 0, GEX_EVENT_NOW, 0, id);
}

void reply_handler(gex_Token_t token, void *buf, size_t nbytes, gex_AM_Arg_t id) {
  receiveds[id]++;
}

void worker(int id, int64_t sleep_us) {
  int issued = 0;
  int num_ops;
  if (sleep_us > 0)
    num_ops = 100000 / sleep_us;
  else
    num_ops = 100000;
  SleepPayload sleep_payload{sleep_us, Payload<56>{}};
  std::default_random_engine generator(arl::backend::rank_me());
  std::uniform_int_distribution<int> distribution(0, arl::backend::rank_n()-1);
  distribution(generator);
  arl::SimpleTimer timer_total;

  threadBarrier.wait();
  if (id == 0) {
    arl::backend::barrier();
  }
  threadBarrier.wait();
  timer_total.start();

  for (size_t i = 0; i < num_ops; i++) {
    size_t remote_proc = distribution(generator);
    int rv = gex_AM_RequestMedium(arl::backend::internal::tm, remote_proc, req_num, &sleep_payload, sizeof(sleep_payload), GEX_EVENT_NOW, 0, id);
    issued++;
  }

  while (receiveds[id] < issued) {
    gasnet_AMPoll();
  }
  threadBarrier.wait();
  if (id == 0) {
    arl::backend::barrier();
  }
  threadBarrier.wait();
  timer_total.end();

  if (arl::backend::rank_me() == 0 && id == 0) {
    printf("sleep time is %ld us\n", sleep_us);
    printf("GASNet overhead is %.2lf us (total %.2lf s)\n", timer_total.to_us() / num_ops, timer_total.to_s());
  }
}

template <typename Fn, typename... Args>
void run(Fn &&fn, Args &&... args) {
  using fn_t = decltype(+std::declval<std::remove_reference_t<Fn>>());
  std::vector<std::thread> worker_pool;
  for (size_t i = 0; i < 16; ++i) {
    auto t = std::thread(std::forward<fn_t>(+fn), i, std::forward<Args>(args)...);
    worker_pool.push_back(std::move(t));
  }

  for (size_t i = 0; i < 16; ++i) {
    worker_pool[i].join();
  }
}

int main() {
  arl::backend::init(2048);

  size_t max_args = gex_AM_MaxArgs();
  size_t handler_num = GEX_AM_INDEX_BASE + 1;
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
  run(worker, 0);
  run(worker, 1);
  run(worker, 10);
  run(worker, 30);
  run(worker, 100);
  run(worker, 300);
  run(worker, 1000);

  arl::backend::finalize();
  return 0;

}