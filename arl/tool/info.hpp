#ifndef ARL_INFO_HPP
#define ARL_INFO_HPP

namespace arl::info {
#ifdef ARL_INFO
class InfoCounter {
  using size_type = int64_t;
  size_type counter = 0;
 public:
  void add(size_type n) {
    counter += n;
  }
  void reset() {
    counter = 0;
  }
  [[nodiscard]] size_type get() const {
    return counter;
  }
};
#else
class InfoCounter {
  using size_type = int64_t;
 public:
  void add(size_type n) {}
  void reset() {}
  [[nodiscard]] size_type get() const {return -1;}
};
#endif

struct NetworkInfo {
  InfoCounter byte_send;
  InfoCounter byte_recv;
  void reset() {
    byte_send.reset();
    byte_recv.reset();
  }
};

__thread NetworkInfo networkInfo;
} // namespace arl::info

#endif //ARL_INFO_HPP
