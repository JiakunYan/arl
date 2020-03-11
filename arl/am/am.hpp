//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_HPP
#define ARL_AM_HPP

namespace arl {
namespace am_internal {
// Get a *position independent* function pointer
template<typename T>
std::intptr_t get_pi_fnptr(T *fn) {
  return reinterpret_cast<std::intptr_t>(fn) - reinterpret_cast<std::intptr_t>(init);
}

// Resolve a *position independent* function pointer
// to a local function pointer.
template<typename T>
T *resolve_pi_fnptr(std::uintptr_t fn) {
  return reinterpret_cast<T *>(fn + reinterpret_cast<std::uintptr_t>(init));
}

alignas(alignof_cacheline) int gex_am_handler_num;
// AM synchronous counters
alignas(alignof_cacheline) std::atomic<int64_t> *am_ack_counter;
alignas(alignof_cacheline) std::atomic<int64_t> *am_req_counter;

extern void init_am_ff();
extern void exit_am_ff();
extern void flush_am_ff_buffer();

void init_am() {
  am_ack_counter = new std::atomic<int64_t>;
  *am_ack_counter = 0;
  am_req_counter = new std::atomic<int64_t>;
  *am_req_counter = 0;

  init_am_ff();
}

void exit_am() {
  exit_am_ff();
  delete am_ack_counter;
  delete am_req_counter;
}

} // namespace am_internal

void flush_agg_buffer() {
  am_internal::flush_am_ff_buffer();
}

void flush_am() {
  while (*am_internal::am_req_counter > *am_internal::am_ack_counter) {
    progress();
  }
}

void progress() {
  gasnet_AMPoll();
}
} // namespace arl

#endif //ARL_AM_HPP
