#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl::am_internal {
inline void do_something() {
  //  progress();
}

class AggBuffer {
  public:
  using size_type = int64_t;
  virtual ~AggBuffer() = default;
  virtual void init(size_type cap, size_type prefix) = 0;
  virtual std::pair<char *, size_type> push(char *p1, size_t s1,
                                            char *p2, size_t s2) = 0;
  virtual std::pair<char *, size_type> push(char *p, size_t s) = 0;
  virtual std::pair<char *, size_type> reserve(size_t, char**) = 0;
  virtual void commit(size_t s) = 0;
  virtual std::vector<std::pair<char *, size_type>> flush() = 0;
  [[nodiscard]] virtual size_type get_size() const = 0;
  [[nodiscard]] virtual std::string get_status() const = 0;
};
}// namespace arl::am_internal

#include "agg_buffer_atomic.hpp"
#include "agg_buffer_local.hpp"
#include "agg_buffer_simple.hpp"

#endif//ARL_AGG_BUFFER_HPP
