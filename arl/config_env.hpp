#ifndef ARL_CONFIG_ENV_HPP
#define ARL_CONFIG_ENV_HPP

namespace arl::config {
enum AggBufferType {
  AGG_BUFFER_SIMPLE = 0,
  AGG_BUFFER_LOCAL,
  AGG_BUFFER_ATOMIC
} aggBufferType;

static const char * const aggBufferTypeToString[] = {
        "simple",
        "local",
        "atomic",
};

void init_aggBufferType() {
  char *p = getenv("ARL_AGG_BUFFER_TYPE");
  if (p == nullptr || strcmp(p, aggBufferTypeToString[AGG_BUFFER_ATOMIC]) == 0)
    aggBufferType = AGG_BUFFER_ATOMIC;
  else if (strcmp(p, aggBufferTypeToString[AGG_BUFFER_SIMPLE]) == 0)
    aggBufferType = AGG_BUFFER_SIMPLE;
  else if (strcmp(p, aggBufferTypeToString[AGG_BUFFER_LOCAL]) == 0)
    aggBufferType = AGG_BUFFER_LOCAL;
  else
    fprintf(stderr, "unknown env LCM_LOG_LEVEL (%s against simple|local|atomic). use the default atomic.\n", p);
}

void init() {
  init_aggBufferType();
}
} // namespace arl::config

#endif//ARL_CONFIG_ENV_HPP
