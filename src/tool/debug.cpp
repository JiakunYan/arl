#include "arl_internal.hpp"

namespace arl::debug {
void arl_traceback(FILE *fp) {
  void *array[20];
  size_t size;
  size = backtrace(array, 20);

  // print out all the frames to stderr
  if (size != 0) {
    int fd;
    if (fp == nullptr) {
      fd = STDERR_FILENO;
    } else {
      fd = fileno(fp);
    }
    backtrace_symbols_fd(array, size, fd);
  }
}

void default_timeout_handler() {
  FILE *fp = fopen(string_format("traceback.arl_dlog.", rank_me()).c_str(), "w");
  fprintf(fp, "Rank %ld\n", rank_me());
  //  fprintf(fp, "AggBuffer status:\n%s\n", amaggrd_internal::get_amaggrd_buffer_status().c_str());
  fflush(fp);
  arl_traceback(fp);
}

void set_timeout(double t) {
  timeout = t;
}
} // namespace arl::debug