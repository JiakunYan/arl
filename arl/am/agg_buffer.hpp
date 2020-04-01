//
// Created by jackyan on 3/11/20.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl{
extern void progress();
namespace am_internal {

// The AggBuffer data structure is one of the most important component in the node-level aggregation
// architecture of ARL.

// A simple aggregation buffer, slow but should hardly have bugs.
// All the worker threads share one memory chunk.
// Use one mutex lock to ensure its thread-safety.
// flush() will trivially flush the whole aggregation buffer, because the buffer only has one memory chunk.
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

  template <typename T, typename U>
  std::pair<char*, int> push(const T& val1, const U& val2) {
    while (!lock.try_lock()) {
      progress();
    }
    std::pair<char*, int> result(nullptr, 0);
    if (tail_ + sizeof(val1) + sizeof(val2) > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &val1, sizeof(T));
    tail_ += sizeof(val1);
    std::memcpy(ptr_ + tail_, &val2, sizeof(U));
    tail_ += sizeof(val2);
    lock.unlock();
    return result;
  }

  template <typename T>
  std::pair<char*, int> push(const T& value) {
    while (!lock.try_lock()) {
      progress();
    }
    std::pair<char*, int> result(nullptr, 0);
    if (tail_ + sizeof(value) > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &value, sizeof(T));
    tail_ += sizeof(value);
    lock.unlock();
    return result;
  }

  std::vector<std::pair<char*, int>> flush() {
    std::vector<std::pair<char*, int>> result;
    if (tail_ == 0) return result;
    while (!lock.try_lock()) {
      progress();
      if (tail_ == 0) return result;
    }
    if (tail_ > 0) {
      result.emplace_back(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    lock.unlock();
    return result;
  }

 private:
  alignas(alignof_cacheline) char* ptr_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) int tail_;
  alignas(alignof_cacheline) std::mutex lock;
};

// A thread-local aggregation buffer, no contention between threads.
// Each thread has its own memory chunk.
// flush() only flushes the caller's local memory chunk.
class AggBufferLocal {
 public:
  AggBufferLocal(): cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete [] thread_tail_;
    thread_tail_ = new AlignedInt [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i].val = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i].val;
      }
      delete [] thread_ptr_;
    }
    thread_ptr_ = new AlignedCharPtr[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i].val = new char[cap];
    }
  }

  ~AggBufferLocal() {
    delete [] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i].val;
      }
      delete [] thread_ptr_;
    }
  }

  template <typename T, typename U>
  std::pair<char*, int> push(const T& val1, const U& val2) {
    int my_rank = local::rank_me();
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val1, sizeof(val1));
    thread_tail_[my_rank].val += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val2, sizeof(val2));
    thread_tail_[my_rank].val += sizeof(val2);
    return result;
  }

  template <typename T>
  std::pair<char*, int> push(const T& value) {
    int my_rank = local::rank_me();
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(value) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &value, sizeof(T));
    thread_tail_[my_rank].val += sizeof(value);
    return result;
  }

  std::vector<std::pair<char*, int>> flush() {
//    std::vector<std::pair<int, int>> data;
//    for (int i = 0; i < thread_num_; ++i) {
//      data.emplace_back(i, thread_tail_[i]);
//    }
//    std::sort(data.begin(), data.end(),
//              [](std::pair<int, int> a, std::pair<int, int> b) {
//                return (std::get<1>(a) < std::get<1>(b));
//              });
    int my_rank = local::rank_me();
    std::vector<std::pair<char*, int>> results;
    if (thread_tail_[my_rank].val > 0) {
      results.emplace_back(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = 0;
    }
    return results;
  }

 private:
  struct AlignedCharPtr {
    alignas(alignof_cacheline) char* val;
  };
  alignas(alignof_cacheline) AlignedCharPtr* thread_ptr_;
  struct AlignedInt {
    alignas(alignof_cacheline) int val;
  };
  alignas(alignof_cacheline) AlignedInt* thread_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) int thread_num_;
}; // AggBufferLocal

// A process-level aggregation buffer, light contention between threads
// Each thread has its own memory chunk.
// Each memory chunk has an associated mutex lock.
// flush() will flush all the memory chunks, and thus cost more time.
class AggBufferAdvanced {
 public:
  AggBufferAdvanced(): cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr), lock_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete [] lock_ptr_;
    lock_ptr_ = new AlignedMutex [thread_num_];

    delete [] thread_tail_;
    thread_tail_ = new AlignedInt [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i].val = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i].val;
      }
      delete [] thread_ptr_;
    }
    thread_ptr_ = new AlignedCharPtr [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i].val = new char[cap];
    }
  }

  ~AggBufferAdvanced() {
    delete [] lock_ptr_;
    delete [] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i].val;
      }
      delete [] thread_ptr_;
    }
  }

  template <typename T, typename U>
  std::pair<char*, int> push(const T& val1, const U& val2) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].val.try_lock()) {
      progress();
    }
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val1, sizeof(val1));
    thread_tail_[my_rank].val += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val2, sizeof(val2));
    thread_tail_[my_rank].val += sizeof(val2);
    lock_ptr_[my_rank].val.unlock();
    return result;
  }

  template <typename T>
  std::pair<char*, int> push(const T& value) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].val.try_lock()) {
      progress();
    }
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(value) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &value, sizeof(T));
    thread_tail_[my_rank].val += sizeof(value);
    lock_ptr_[my_rank].val.unlock();
    return result;
  }

  std::vector<std::pair<char*, int>> flush() {
//    std::vector<std::pair<int, int>> data;
//    for (int i = 0; i < thread_num_; ++i) {
//      data.emplace_back(i, thread_tail_[i]);
//    }
//    std::sort(data.begin(), data.end(),
//              [](std::pair<int, int> a, std::pair<int, int> b) {
//                return (std::get<1>(a) < std::get<1>(b));
//              });
    int my_rank = local::rank_me();
    std::vector<std::pair<char*, int>> results;
    for (int ii = 0; ii < thread_num_; ++ii) {
      int i = (ii + my_rank) % thread_num_;
      if (thread_tail_[i].val == 0) continue;
      while (!lock_ptr_[i].val.try_lock()) {
        progress();
        if (thread_tail_[i].val == 0) continue;
      }
      if (thread_tail_[i].val > 0) {
        results.emplace_back(thread_ptr_[i].val, thread_tail_[i].val);
        thread_ptr_[i].val = new char [cap_];
        thread_tail_[i].val = 0;
      }
      lock_ptr_[i].val.unlock();
    }
    return results;
  }

 private:
  struct AlignedCharPtr {
    alignas(alignof_cacheline) char* val;
  };
  alignas(alignof_cacheline) AlignedCharPtr* thread_ptr_;
  struct AlignedInt {
    alignas(alignof_cacheline) int val;
  };
  alignas(alignof_cacheline) AlignedInt* thread_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) int thread_num_;
  struct AlignedMutex {
    alignas(alignof_cacheline) std::mutex val;
  };
  alignas(alignof_cacheline) AlignedMutex* lock_ptr_;
}; // class AggBufferAdvanced

class AggBufferAtomic {
 public:
  AggBufferAtomic() : cap_(0), tail_(0), reserved_tail_(0), ptr_(nullptr) {}

  ~AggBufferAtomic() {
    delete [] ptr_;
  }

  void init(int cap) {
    ARL_Assert(cap > 0);
//    cap_ = cap / sizeof(T) * sizeof(T);
    cap_ = cap;
    delete [] ptr_;
    ptr_ = new char[cap_];
    reserved_tail_ = 0;
    tail_ = 0;
  }

  template <typename T>
  std::pair<char*, int> push(const T &val) {
    static_assert(std::is_trivially_copyable<T>::value);

    std::pair<char*, int> result(nullptr, 0);
    int current_tail = tail_.fetch_add(sizeof(val));
    while (current_tail > cap_) {
      progress();
      current_tail = tail_.fetch_add(sizeof(val));
      ARL_Assert(current_tail >= 0, "AggBuffer: tail overflow!");
    }
    if (current_tail <= cap_ && current_tail + sizeof(val) > cap_) {
      while (!mutex_pop_.try_lock()) {
        progress();
      }

      while (reserved_tail_ != current_tail) {
        progress();
      }
      result = std::make_pair(ptr_, current_tail);
      ptr_ = new char[cap_];
      reserved_tail_ = 0;
      tail_ = sizeof(val);

      mutex_pop_.unlock();
      current_tail = 0;
    }
    std::memcpy(ptr_ + current_tail, &val, sizeof(val));
    reserved_tail_.fetch_add(sizeof(val));
    return result;
  }

  std::vector<std::pair<char*, int>> flush() {
    std::vector<std::pair<char*, int>> result;
    if (tail_.load() == 0) {
      return result;
    }
    if (!mutex_pop_.try_lock()) {
      return result;
    }
    int current_tail = tail_.fetch_add(cap_ + 1); // prevent others from begining pushing
    if (current_tail <= cap_ && current_tail > 0) {
      // wait until those who is pushing finish
//      printf("wait for flush %d, %d\n", reserved_tail_.load(), current_tail);
      while (reserved_tail_ != current_tail) {
        progress();
      }

      result.emplace_back(ptr_, current_tail);
      ptr_ = new char[cap_];
      reserved_tail_ = 0;
      tail_ = 0;
    } // else if (current_tail > cap_): some push will flush the buffer
    mutex_pop_.unlock();
    return result;
  }

 private:
  alignas(alignof_cacheline) char* ptr_;
  alignas(alignof_cacheline) std::atomic<int> tail_;
  alignas(alignof_cacheline) std::atomic<int> reserved_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) std::mutex mutex_pop_;
}; // class AggBufferAtomic

//using AggBuffer = AggBufferSimple;
//using AggBuffer = AggBufferLocal;
using AggBuffer = AggBufferAdvanced;

} // namespace am_internal
} // namespace arl

#endif //ARL_AGG_BUFFER_HPP
