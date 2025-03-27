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

enum AggBufferType {
  AGG_BUFFER_SIMPLE = 0,
  AGG_BUFFER_LOCAL,
  AGG_BUFFER_ATOMIC
};
extern AggBufferType aggBufferType;

extern void init();
}// namespace arl::config

#endif//ARL_CONFIG_ENV_HPP
