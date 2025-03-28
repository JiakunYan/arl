#include "arl_internal.hpp"

namespace arl::config {

log_level_t log_level = log_level_t::WARN;
bool pin_thread = true;
size_t max_buffer_size = 1 << 31;
AggBufferType aggBufferType = AGG_BUFFER_LOCAL;
bool lci_shared_cq = false;
int lci_ndevices = -1;
extern bool backend_only_mode = false;
bool rpc_loopback = true;
bool msg_loopback = true;

static const char *const aggBufferTypeToString[] = {
        "simple",
        "local",
        "atomic",
};

void init() {
  // log level
  char *p = getenv("ARL_LOG_LEVEL");
  if (p) {
    if (strcmp(p, "warn") == 0)
      log_level = log_level_t::WARN;
    else if (strcmp(p, "info") == 0)
      log_level = log_level_t::INFO;
    else if (strcmp(p, "debug") == 0)
      log_level = log_level_t::DEBUG;
    else
      fprintf(stderr, "unknown env ARL_LOG_LEVEL (%s against warn|info|debug). use the default warn.\n", p);

    const char *log_level_str[] = {"warn", "info", "debug"};
    ARL_LOG(INFO, "ARL log level: %s\n", log_level_str[static_cast<int>(log_level)]);
  }

  // pin thread
  p = getenv("ARL_PIN_THREAD");
  if (p) {
    pin_thread = atoi(p);
    ARL_LOG(INFO, "ARL pin thread: %d\n", pin_thread);
  }

  // max buffer size
  p = getenv("ARL_MAX_BUFFER_SIZE");
  if (p) {
    max_buffer_size = static_cast<size_t>(std::stoull(p));
    ARL_LOG(INFO, "ARL max buffer size: %zu\n", max_buffer_size);
  }

  // agg buffer type
  p = getenv("ARL_AGG_BUFFER_TYPE");
  if (p) {
    if (strcmp(p, aggBufferTypeToString[AGG_BUFFER_ATOMIC]) == 0)
      aggBufferType = AGG_BUFFER_ATOMIC;
    else if (strcmp(p, aggBufferTypeToString[AGG_BUFFER_SIMPLE]) == 0)
      aggBufferType = AGG_BUFFER_SIMPLE;
    else if (strcmp(p, aggBufferTypeToString[AGG_BUFFER_LOCAL]) == 0)
      aggBufferType = AGG_BUFFER_LOCAL;
    else
      fprintf(stderr, "unknown env LCM_LOG_LEVEL (%s against simple|local|atomic). use the default atomic.\n", p);
    ARL_LOG(INFO, "ARL agg buffer type: %s\n", aggBufferTypeToString[static_cast<int>(aggBufferType)]);
  }

  // lci shared cq
  p = getenv("ARL_LCI_SHARED_CQ");
  if (p) {
    lci_shared_cq = atoi(p);
    ARL_LOG(INFO, "ARL lci shared cq: %d\n", lci_shared_cq);
  }

  // lci ndevices
  p = getenv("ARL_LCI_NDEVICES");
  if (p) {
    lci_ndevices = atoi(p);
    ARL_LOG(INFO, "ARL lci ndevices: %d\n", lci_ndevices);
  }

  // backend only mode
  p = getenv("ARL_BACKEND_ONLY_MODE");
  if (p) {
    backend_only_mode = atoi(p);
    ARL_LOG(INFO, "ARL backend only mode: %d\n", backend_only_mode);
  }

  // rpc loopback
  p = getenv("ARL_RPC_LOOPBACK");
  if (p) {
    rpc_loopback = atoi(p);
    ARL_LOG(INFO, "ARL rpc loopback: %d\n", rpc_loopback);
  }

  // msg loopback
  p = getenv("ARL_MSG_LOOPBACK");
  if (p) {
    msg_loopback = atoi(p);
    ARL_LOG(INFO, "ARL msg loopback: %d\n", msg_loopback);
  }
}
}// namespace arl::config