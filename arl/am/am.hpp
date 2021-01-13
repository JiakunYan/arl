//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_HPP
#define ARL_AM_HPP

namespace arl {

namespace amagg_internal {
extern void init_amagg();
extern void exit_amagg();
extern void flush_amagg_buffer();
extern void wait_amagg();
extern void generic_amagg_reqhandler(const backend::cq_entry_t& event);
extern void generic_amagg_ackhandler(const backend::cq_entry_t& event);
} // namespace amagg_internal

namespace amff_internal {
extern void init_amff();
extern void exit_amff();
extern void flush_amff_buffer();
extern void wait_amff();
extern void generic_amff_reqhandler(const backend::cq_entry_t& event);
} // namespace amff_internal

namespace amaggrd_internal {
extern void init_amaggrd();
extern void exit_amaggrd();
extern void flush_amaggrd_buffer();
extern void wait_amaggrd();
extern void generic_amaggrd_reqhandler(const backend::cq_entry_t& event);
extern void generic_amaggrd_ackhandler(const backend::cq_entry_t& event);
} // amffrd_internal

namespace amffrd_internal {
extern void init_amffrd();
extern void exit_amffrd();
extern void flush_amffrd_buffer();
extern void wait_amffrd();
extern void generic_amffrd_reqhandler(const backend::cq_entry_t& event);
} // amffrd_internal

namespace am_internal {
extern inline bool pool_am_event_queue();

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

inline void init_am() {
  amagg_internal::init_amagg();
  amff_internal::init_amff();
  amaggrd_internal::init_amaggrd();
  amffrd_internal::init_amffrd();
}

inline void exit_am() {
  amffrd_internal::exit_amffrd();
  amaggrd_internal::exit_amaggrd();
  amff_internal::exit_amff();
  amagg_internal::exit_amagg();
}

} // namespace am_internal

void flush_agg_buffer(char rpc_type) {
  if (rpc_type & RPC_AGG)
    amagg_internal::flush_amagg_buffer();
  if (rpc_type & RPC_FF)
    amff_internal::flush_amff_buffer();
  if (rpc_type & RPC_AGGRD)
    amaggrd_internal::flush_amaggrd_buffer();
  if (rpc_type & RPC_FFRD)
    amffrd_internal::flush_amffrd_buffer();
}

void wait_am(char rpc_type) {
  if (rpc_type & RPC_AGG)
    amagg_internal::wait_amagg();
  if (rpc_type & RPC_FF)
    amff_internal::wait_amff();
  if (rpc_type & RPC_AGGRD)
    amaggrd_internal::wait_amaggrd();
  if (rpc_type & RPC_FFRD)
    amffrd_internal::wait_amffrd();
}

void flush_am(char rpc_type) {
  flush_agg_buffer(rpc_type);
  wait_am(rpc_type);
}

inline void progress_internal() {
  backend::progress();
}

// return value indicates whether this function actually executes some works.
inline bool progress_external() {
  return am_internal::pool_am_event_queue();
}

// return value indicates whether this function actually executes some works.
bool progress() {
  progress_internal();
  return progress_external();
}

void progress_until(const std::function<bool()>& is_ready) {
  tick_t start = ticks_now();
  while (!is_ready()) {
    bool active = progress();
    if (active)
      start = ticks_now();
    else if (debug::timeout > 0 && ticks_to_s(ticks_now() - start) > debug::timeout) {
      ARL_TIMEOUT_HANDLER();
      sleep(debug::timeout); // wait for all the other workers
      throw std::runtime_error("progress_until: timeout\n");
    }
  }
}

int get_agg_size() {
  return backend::get_max_buffer_size();
}
} // namespace arl

#endif //ARL_AM_HPP
