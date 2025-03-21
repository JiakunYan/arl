#include "arl_internal.hpp"

namespace arl::backend::internal {

thread_state_t thread_state;

void init() {
  lci::g_runtime_init_x();
  thread_state.device = lci::get_default_device();
  thread_state.endpoint = lci::get_default_endpoint();
  thread_state.comp = lci::alloc_cq();
  thread_state.rcomp = lci::register_rcomp(thread_state.comp);
}

void finalize() {
  lci::free_comp(&thread_state.comp);
  lci::g_runtime_fina();
}

}// namespace arl::backend::internal
