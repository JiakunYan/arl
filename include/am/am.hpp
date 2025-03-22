//
// Created by jackyan on 3/10/20.
//

#ifndef ARL_AM_HPP
#define ARL_AM_HPP

namespace arl {

namespace amagg_internal {
void init_amagg();
void exit_amagg();
void flush_amagg_buffer();
void wait_amagg();
void generic_amagg_reqhandler(const backend::cq_entry_t &event);
void generic_amagg_ackhandler(const backend::cq_entry_t &event);
}// namespace amagg_internal

namespace amff_internal {
extern void init_amff();
extern void exit_amff();
extern void flush_amff_buffer();
extern void wait_amff();
extern void generic_amff_reqhandler(const backend::cq_entry_t &event);
}// namespace amff_internal

namespace amaggrd_internal {
extern void init_amaggrd();
extern void exit_amaggrd();
extern void flush_amaggrd_buffer();
extern void wait_amaggrd();
extern void generic_amaggrd_reqhandler(const backend::cq_entry_t &event);
extern void generic_amaggrd_ackhandler(const backend::cq_entry_t &event);
}// namespace amaggrd_internal

namespace amffrd_internal {
extern void init_amffrd();
extern void exit_amffrd();
extern void flush_amffrd_buffer();
extern void wait_amffrd();
extern void generic_amffrd_reqhandler(const backend::cq_entry_t &event);
}// namespace amffrd_internal

namespace am_internal {

enum HandlerType {
  AM_REQ,
  AM_ACK,
  AM_RD_REQ,
  AM_RD_ACK,
  AM_FF_REQ,
  AM_FFRD_REQ
};

extern inline bool pool_am_event_queue();

// Get a *position independent* function pointer
template<typename T>
std::intptr_t get_pi_fnptr(T *fn) {
  auto ret = reinterpret_cast<std::intptr_t>(fn) - reinterpret_cast<std::intptr_t>(init);
  fprintf(stderr, "rank %ld: get_pi_fnptr: %p, %p, %ld\n", rank_me(), fn, init, ret);
  return ret;
}

// Resolve a *position independent* function pointer
// to a local function pointer.
template<typename T>
T *resolve_pi_fnptr(std::uintptr_t fn) {
  auto ret = reinterpret_cast<T *>(fn + reinterpret_cast<std::uintptr_t>(init));
  fprintf(stderr, "rank %ld: resolve_pi_fnptr: %ld, %p, %p\n", rank_me(), fn, init, ret);
  return ret;
}

extern void init_am();
extern void exit_am();

}// namespace am_internal

inline void flush_agg_buffer(char rpc_type) {
  if (rpc_type & RPC_AGG)
    amagg_internal::flush_amagg_buffer();
  if (rpc_type & RPC_FF)
    amff_internal::flush_amff_buffer();
  if (rpc_type & RPC_AGGRD)
    amaggrd_internal::flush_amaggrd_buffer();
  if (rpc_type & RPC_FFRD)
    amffrd_internal::flush_amffrd_buffer();
}

inline void wait_am(char rpc_type) {
  if (rpc_type & RPC_AGG)
    amagg_internal::wait_amagg();
  if (rpc_type & RPC_FF)
    amff_internal::wait_amff();
  if (rpc_type & RPC_AGGRD)
    amaggrd_internal::wait_amaggrd();
  if (rpc_type & RPC_FFRD)
    amffrd_internal::wait_amffrd();
}

inline void flush_am(char rpc_type) {
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

inline void progress_external_until(const std::function<bool()> &is_ready) {
  tick_t start = ticks_now();
  while (!is_ready()) {
    bool active = progress_external();
    if (active)
      start = ticks_now();
    else if (debug::timeout > 0 && ticks_to_s(ticks_now() - start) > debug::timeout) {
      ARL_TIMEOUT_HANDLER();
      sleep(debug::timeout);// wait for all the other workers
      throw std::runtime_error("progress_external_until: timeout\n");
    }
  }
}

inline int get_agg_size() {
  return backend::get_max_buffer_size();
}
}// namespace arl

#endif//ARL_AM_HPP
