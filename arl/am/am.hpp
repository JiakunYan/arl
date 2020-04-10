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
extern void wait_amagg();
} // namespace amagg_internal

namespace amff_internal {
extern void init_am_ff();
extern void exit_am_ff();
extern void flush_am_ff_buffer();
extern void wait_amff();
} // namespace amff_internal

namespace amaggrd_internal {
extern void init_amaggrd();
extern void exit_amaggrd();
extern void flush_amaggrd();
extern void wait_amaggrd();
} // amffrd_internal

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

void init_am() {
  gex_am_handler_num = GEX_AM_INDEX_BASE;
  amagg_internal::init_amagg();
  amff_internal::init_am_ff();
  amaggrd_internal::init_amaggrd();
  amffrd_internal::init_amffrd();
}

void exit_am() {
  amffrd_internal::exit_amffrd();
  amaggrd_internal::exit_amaggrd();
  amff_internal::exit_am_ff();
  amagg_internal::exit_amagg();
}

} // namespace am_internal

void flush_agg_buffer() {
  amagg_internal::flush_amagg();
  amff_internal::flush_am_ff_buffer();
  amaggrd_internal::flush_amaggrd();
  amffrd_internal::flush_amffrd();

}

void flush_am() {
  amagg_internal::wait_amagg();
  amff_internal::wait_amff();
  amaggrd_internal::wait_amaggrd();
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
