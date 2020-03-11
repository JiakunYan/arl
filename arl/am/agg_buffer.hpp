//
// Created by jackyan on 3/11/20.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl{
extern void progress();
namespace am_internal {

class AggBufferSimple {
 public:
  AggBufferSimple(): cap_(0), tail_(0), ptr_(nullptr) {
  }

  void init(int cap) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    tail_ = 0;
    delete [] ptr_;
    ptr_ = new char[cap];
  }

  ~AggBufferSimple() {
    delete [] ptr_;
  }

  template <typename T>
  pair<char*, int> push(const T& value) {
    while (!lock.try_lock()) {
      progress();
    }
    pair<char*, int> result(nullptr, 0);
    if (tail_ + sizeof(value) > cap_) {
      result = make_pair(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &value, sizeof(T));
    tail_ += sizeof(value);
    lock.unlock();
    return result;
  }

  pair<char*, int> flush() {
    while (!lock.try_lock()) {
      progress();
    }
    pair<char*, int> result = make_pair(ptr_, tail_);
    ptr_ = new char [cap_];
    tail_ = 0;
    lock.unlock();
    return result;
  }

 private:
  char* ptr_;
  int cap_;
  int tail_;
  mutex lock;
};

} // namespace am_internal
} // namespace arl

#endif //ARL_AGG_BUFFER_HPP
