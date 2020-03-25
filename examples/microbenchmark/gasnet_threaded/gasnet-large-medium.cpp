// Show GASNet-ex Bug #4042: AM errors at max-MaxMedium

#include <gasnetex.h>
#include <queue>
#include <mutex>
#include <thread>
#include <cassert>
#include "arh_thread_barrier.hpp"
#include <random>
// GASNet-EX context variables

gex_Client_t client;
gex_EP_t ep;
gex_TM_t tm;
const char* clientName = "ARL";
const int alignof_cacheline = 64;

int rank, nprocs;
int max_buffer_size;

// Process-level variables for thread synchronization
alignas(alignof_cacheline) gex_AM_Index_t hidx_reqhandler;
alignas(alignof_cacheline) gex_AM_Index_t hidx_ackhandler;
// AM synchronous counters
alignas(alignof_cacheline) std::atomic<int64_t> *am_ack_counter;
alignas(alignof_cacheline) std::atomic<int64_t> *am_req_counter;

void barrier() {
  gex_Event_t event = gex_Coll_BarrierNB(tm, 0);
  gex_Event_Wait(event);
}

void reqhandler(gex_Token_t token, void *void_buf, size_t unbytes) {
  int n = unbytes / 64;
  printf("rank %d reqhandler: send n %d\n", rank, n);
  gex_AM_ReplyShort1(token, hidx_ackhandler, 0, static_cast<gex_AM_Arg_t>(n));
}

void ackhandler(gex_Token_t token, gex_AM_Arg_t n) {
  printf("rank %d ackhandler: get n %d\n", rank, static_cast<int>(n));
  *am_ack_counter += 1;
}

void worker() {

  size_t num_ams = 1;

  std::default_random_engine generator(rank);
  std::uniform_int_distribution<int> distribution(0, nprocs-1);
  char* ptr[65536];

  barrier();

  for (size_t i = 0; i < num_ams; i++) {
//    int remote_proc = distribution(generator);
    int remote_proc = (rank + 1) % nprocs;

    *am_ack_counter += 1;
    printf("rank %d send req to rank %d\n", rank, remote_proc);
    gex_AM_RequestMedium0(tm, remote_proc, hidx_reqhandler,
                          ptr, max_buffer_size, GEX_EVENT_NOW, 0);
  }

  while (*am_ack_counter < *am_req_counter) {
    gasnet_AMPoll();
  }

  barrier();
}


int main() {
  am_ack_counter = new std::atomic<int64_t>;
  *am_ack_counter = 0;
  am_req_counter = new std::atomic<int64_t>;
  *am_req_counter = 0;

  gex_Client_Init(&client, &ep, &tm, clientName, NULL, NULL, 0);

#ifndef GASNET_PAR
  printf("Need to use a par build of GASNet-EX.\n");
  assert(false);
#endif

  rank = gex_System_QueryJobRank();
  nprocs = gex_System_QueryJobSize();
  max_buffer_size = gex_AM_MaxRequestMedium(tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
  if (rank == 0) {
    printf("Max buffer size is %d\n", max_buffer_size);
  }

  size_t handler_num = GEX_AM_INDEX_BASE;
  hidx_reqhandler = handler_num++;
  hidx_ackhandler = handler_num++;

  gex_AM_Entry_t htable[2] = {
      { hidx_reqhandler,
          (gex_AM_Fn_t) reqhandler,
          GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST,
          0 },
      { hidx_ackhandler,
          (gex_AM_Fn_t) ackhandler,
          GEX_FLAG_AM_SHORT | GEX_FLAG_AM_REPLY,
          1 } };

  gex_EP_RegisterHandlers(ep, htable, sizeof(htable)/sizeof(gex_AM_Entry_t));

  worker();

  delete am_ack_counter;
  delete am_req_counter;

  return 0;
}
