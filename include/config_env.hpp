#ifndef ARL_CONFIG_ENV_HPP
#define ARL_CONFIG_ENV_HPP

namespace arl::config {
enum class log_level_t {
  WARN,
  INFO,
  DEBUG,
};
extern log_level_t log_level;

extern bool pin_thread;
extern size_t max_buffer_size;
extern int lci_ndevices;
extern bool lci_shared_cq;
extern bool lci_shared_progress;
extern size_t lci_max_sends;
extern size_t lci_min_sends;
extern bool backend_only_mode;
extern bool rpc_loopback;
extern bool msg_loopback;

enum AggBufferType {
  AGG_BUFFER_SIMPLE = 0,
  AGG_BUFFER_LOCAL,
  AGG_BUFFER_ATOMIC
};
extern AggBufferType aggBufferType;

extern void init();
}// namespace arl::config

#endif//ARL_CONFIG_ENV_HPP
