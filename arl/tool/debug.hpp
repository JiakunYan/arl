//
// Created by jiakun on 2020/9/22.
//

#ifndef ARL_TRACEBACK_HPP
#define ARL_TRACEBACK_HPP

namespace arl::debug {
void arl_traceback() {
  void *array[20];
  size_t size;
  size = backtrace(array, 20);

  // print out all the frames to stderr
  if (size != 0) {
    FILE *fp = fopen(string_format("debug_", rank_me(), ".arl_dlog").c_str(), "w");
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
  arl::debug::arl_traceback();
}

inline void set_timeout(double t) {
  timeout = t;
}
} // namespace arl::debug

#endif //ARL_TRACEBACK_HPP
