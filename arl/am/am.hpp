//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_HPP
#define ARL_AM_HPP

namespace arl {

namespace amagg_internal {
extern void init_amagg();
extern void exit_amagg();
extern void flush_amagg();
}

namespace amffrd_internal {
extern void init_amffrd();
extern void exit_amffrd();
extern void flush_amffrd();
extern void wait_amffrd();
} // amffrd_internal

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

  gex_am_handler_num = GEX_AM_INDEX_BASE;
  amagg_internal::init_amagg();
  init_am_ff();
  amffrd_internal::init_amffrd();
}

void exit_am() {
  amffrd_internal::exit_amffrd();
  exit_am_ff();
  amagg_internal::exit_amagg();
  delete am_ack_counter;
  delete am_req_counter;
}

} // namespace am_internal

void flush_agg_buffer() {
  amagg_internal::flush_amagg();
  am_internal::flush_am_ff_buffer();
  amffrd_internal::flush_amffrd();

}

void flush_am() {
  while (*am_internal::am_req_counter > *am_internal::am_ack_counter) {
    progress();
  }
  amffrd_internal::wait_amffrd();
}

void progress() {
  gasnet_AMPoll();
}

int get_agg_size() {
  return gex_AM_MaxRequestMedium(backend::tm,GEX_RANK_INVALID,GEX_EVENT_NOW,0,0);
}
} // namespace arl

#endif //ARL_AM_HPP
