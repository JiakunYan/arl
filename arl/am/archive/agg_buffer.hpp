//
// Created by Jiakun Yan on 10/25/19.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl {
namespace am_internal {

class AggBuffer {
 public:
  enum class Status {
    FAIL,
    SUCCESS,
    FAIL_AND_FULL
  };

  AggBuffer(int size) : cap_(0), tail_(0), reserved_tail_(0), ptr_(nullptr) {}

  ~AggBuffer() {
    delete [] ptr_;
  }

  size_t size() const {
    return reserved_tail_.load();
  }

  template<typename T>
  Status push(const T &val) {
    static_assert(std::is_trivially_copyable<T>::value);
    size_t current_tail = tail_.fetch_add(sizeof(val));
    if (current_tail > cap_) {
      return Status::FAIL;
    } if (current_tail <= cap_ && current_tail + sizeof(val) > cap_) {
      return Status::FAIL_AND_FULL;
    }
    std::memcpy(ptr_ + current_tail, &val, sizeof(val));
    size_t temp = reserved_tail_.fetch_add(sizeof(val));
    return Status::SUCCESS;
  }

  pair<char*, int> pop_all() {
    if (tail_.load() == 0) {
      return make_pair(nullptr, 0);
    }
    if (mutex_pop_.try_lock()) {
      size_t real_tail = std::min(tail_.fetch_add(cap_), cap_); // prevent others from begining pushing
      // wait until those who is pushing finish
      // TODO: fix this bug
      while (reserved_tail_ != real_tail) continue;

      char* result = ptr_;
      ptr_ = new char[cap_];

      ARL_Assert(real_tail <= cap_, "");
      ARL_Assert(real_tail == reserved_tail__, "");

      reserved_tail_ = 0;
      tail_ = 0;

      mutex_pop_.unlock();
      return make_pair(result, real_tail);
    } else {
      // someone is poping
      return make_pair(nullptr, 0);
    }
  }

 private:
  alignas(alignof_cacheline) char* ptr_;
  alignas(alignof_cacheline) std::atomic<int> tail_;
  alignas(alignof_cacheline) std::atomic<int> reserved_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) std::mutex mutex_pop_;
};
} // namespace am_internal
} // namespace arl

#endif //ARL_AGG_BUFFER_HPP
