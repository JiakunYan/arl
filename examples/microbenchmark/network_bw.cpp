#include "arl.hpp"
#include <queue>
#include <mutex>
#include <thread>
#include <random>

bool thread_run = true;
arl::ThreadBarrier threadBarrier;
std::atomic<int> *receiveds;
std::atomic<int> done(0);

bool touch_buffer = false;

bool progress() {
  while (arl::backend::progress() == arl::ARL_OK) continue;

  arl::backend::cq_entry_t event;
  int ret = arl::backend::poll_msg(event);
  if (ret == arl::ARL_RETRY) return false;
  int type = event.tag % 3;
  int thread_id = event.tag / 3;
  if (type == 0) {
    if (touch_buffer) {
      char *buf = (char *)event.buf;
      for (size_t i = 0; i < event.nbytes; i++) {
        if (buf[i] != 'a') {
          fprintf(stderr, "Rank %ld Thread %d received corrupted message from %ld\n", arl::backend::rank_me(), thread_id, event.srcRank);
          break;
        }
        buf[i] = 'b';
      }
    }
    arl::backend::send_msg(event.srcRank, event.tag + 1, event.buf, event.nbytes);
    // fprintf(stderr, "Rank %ld Thread %d received request from %ld\n", arl::backend::rank_me(), thread_id, event.srcRank);
  } else if (type == 1) {
    int n = ++receiveds[thread_id];
    if (touch_buffer) {
      for (size_t i = 0; i < event.nbytes; i++) {
        if (((char *)event.buf)[i] != 'b') {
          fprintf(stderr, "Rank %ld Thread %d received corrupted message from %ld\n", arl::backend::rank_me(), thread_id, event.srcRank);
          break;
        }
      }
    }
    arl::backend::buffer_free(event.buf);
    // fprintf(stderr, "Rank %ld Thread %d received response (%d) from %ld\n", arl::backend::rank_me(), thread_id, n, event.srcRank);
  } else {
    int n = ++done;
    // fprintf(stderr, "Rank %ld Thread %d received done signal (%d) from %ld\n", arl::backend::rank_me(), thread_id, n, event.srcRank);
    arl::backend::buffer_free(event.buf);
  }
  return true;
}

void worker(int nmsgs, size_t payload_size) {
  int thread_id = arl::local::rank_me();
  int nthreads = arl::local::rank_n();
  
  std::default_random_engine generator(arl::backend::rank_me());
  std::uniform_int_distribution<int> distribution(0, arl::backend::rank_n()-1);
  distribution(generator);

  threadBarrier.wait(progress);
  if (thread_id == 0) {
    arl::backend::barrier(progress);
  }
  threadBarrier.wait(progress);
  auto begin = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < nmsgs; i++) {
    size_t remote_proc = distribution(generator);
    void *send_buf = arl::backend::buffer_alloc(payload_size);
    if (touch_buffer) {
      memset(send_buf, 'a', payload_size);
    }
    arl::backend::send_msg(remote_proc, thread_id * 3, send_buf, payload_size);
    while (progress()) continue;
  }

  while (receiveds[thread_id] < nmsgs) {
    progress();
  }
  
  threadBarrier.wait(progress);
  if (thread_id == 0) {
    arl::backend::barrier(progress);
  }
  threadBarrier.wait(progress);
  auto end = std::chrono::high_resolution_clock::now();
  double duration_s = std::chrono::duration<double>(end - begin).count();

  double bandwidth_node_s = payload_size * nmsgs * nthreads * 2 / duration_s;
  if (arl::backend::rank_me() == 0 && thread_id == 0) {
    fprintf(stderr, "Node single-direction bandwidth = %.3lf MB/s\n", bandwidth_node_s / 1e6);
  }
  // fprintf(stderr, "Rank %ld Thread %d finished\n", arl::backend::rank_me(), thread_id);
}

int main(int argc, char **argv) {
  int nthreads = 16;
  int nmsgs = 10000;
  size_t payload_size = 8192;
  if (argc > 1) {
    nthreads = std::atoi(argv[1]);
  }
  if (argc > 2) {
    nmsgs = std::atoi(argv[2]);
  }
  if (argc > 3) {
    payload_size = std::atoi(argv[3]);
  }
  if (argc > 4) {
    touch_buffer = std::atoi(argv[4]);
  }

  arl::init(nthreads, nthreads);
  if (arl::backend::rank_me() == 0) {
    // print config
    fprintf(stderr, "nthreads = %d\n", nthreads);
    fprintf(stderr, "nmsgs = %d\n", nmsgs);
    fprintf(stderr, "payload_size = %lu\n", payload_size);
    fprintf(stderr, "touch_buffer = %d\n", touch_buffer);
  }

  receiveds = new std::atomic<int>[nthreads];
  for (int i = 0; i < nthreads; i++) {
    receiveds[i] = 0;
  }
  payload_size = std::min(arl::backend::get_max_buffer_size(), payload_size);
  if (arl::backend::rank_me() == 0)
    std::printf("Maximum medium payload size is %lu\n", payload_size);

  threadBarrier.init(nthreads);

  // std::vector<std::thread> worker_pool;
  // for (size_t i = 0; i < nthreads; ++i) {
  //   auto t = std::thread(worker, i, nthreads, nmsgs, payload_size);
  //   worker_pool.push_back(std::move(t));
  // }

  // for (size_t i = 0; i < nthreads; ++i) {
  //   worker_pool[i].join();
  // }
  arl::run(worker, nmsgs, payload_size);

  delete[] receiveds;
  arl::backend::finalize();
  return 0;

}