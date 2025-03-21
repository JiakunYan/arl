#ifndef ARL_CONFIG_ENV_HPP
#define ARL_CONFIG_ENV_HPP

namespace arl::config {
enum AggBufferType {
  AGG_BUFFER_SIMPLE = 0,
  AGG_BUFFER_LOCAL,
  AGG_BUFFER_ATOMIC
};
extern AggBufferType aggBufferType;

extern void init();
}// namespace arl::config

#endif//ARL_CONFIG_ENV_HPP
