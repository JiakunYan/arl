//
// Created by jackyan on 3/11/20.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl {
namespace am_internal {
inline void do_something() {
  //  progress();
}
// The AggBuffer data structure is one of the most important component in the node-level aggregation
// architecture of ARL.

// A simple aggregation buffer, slow but should hardly have bugs.
// All the worker threads share one memory chunk.
// Use one mutex lock to ensure its thread-safety.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() will trivially flush the whole aggregation buffer, because the buffer only has one memory chunk.
class AggBufferSimple {
  public:
  using size_type = int64_t;
  AggBufferSimple() : cap_(0), tail_(0), ptr_(nullptr) {
  }

  void init(size_type cap) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    tail_ = 0;
    delete[] ptr_;
    ptr_ = new char[cap];
  }

  ~AggBufferSimple() {
    delete[] ptr_;
  }

  template<typename T, typename U>
  std::pair<char *, size_type> push(const T &val1, const U &val2) {
    while (!lock.try_lock()) {
      do_something();
    }
    std::pair<char *, size_type> result(nullptr, 0);
    if (tail_ + sizeof(val1) + sizeof(val2) > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &val1, sizeof(T));
    tail_ += sizeof(val1);
    std::memcpy(ptr_ + tail_, &val2, sizeof(U));
    tail_ += sizeof(val2);
    lock.unlock();
    return result;
  }

  template<typename T>
  std::pair<char *, size_type> push(const T &value) {
    while (!lock.try_lock()) {
      do_something();
    }
    std::pair<char *, size_type> result(nullptr, 0);
    if (tail_ + sizeof(value) > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &value, sizeof(T));
    tail_ += sizeof(value);
    lock.unlock();
    return result;
  }

  std::vector<std::pair<char *, size_type>> flush() {
    std::vector<std::pair<char *, size_type>> result;
    if (tail_ == 0) return result;
    while (!lock.try_lock()) {
      do_something();
      if (tail_ == 0) return result;
    }
    if (tail_ > 0) {
      result.emplace_back(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = 0;
    }
    lock.unlock();
    return result;
  }

  [[nodiscard]] size_type get_size() const {
    return tail_;
  }

  [[nodiscard]] std::string get_status() const {
    std::ostringstream os;
    os << "ptr_  = " << (void *) ptr_ << '\n';
    os << "tail_ = " << tail_ << "\n";
    os << "cap_  = " << cap_ << "\n";
    return os.str();
  }

  private:
  alignas(alignof_cacheline) char *ptr_;
  alignas(alignof_cacheline) size_type cap_;
  alignas(alignof_cacheline) size_type tail_;
  alignas(alignof_cacheline) std::mutex lock;
};

// A thread-local aggregation buffer, no contention between threads.
// Each thread has its own memory chunk.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() only flushes the caller's local memory chunk.
class AggBufferLocal {
  public:
  AggBufferLocal() : cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete[] thread_tail_;
    thread_tail_ = new AlignedInt[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i].val = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete[] thread_ptr_[i].val;
      }
      delete[] thread_ptr_;
    }
    thread_ptr_ = new AlignedCharPtr[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i].val = new char[cap];
    }
  }

  ~AggBufferLocal() {
    delete[] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete[] thread_ptr_[i].val;
      }
      delete[] thread_ptr_;
    }
  }

  template<typename T, typename U>
  std::pair<char *, int> push(const T &val1, const U &val2) {
    int my_rank = local::rank_me();
    std::pair<char *, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char[cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val1, sizeof(val1));
    thread_tail_[my_rank].val += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val2, sizeof(val2));
    thread_tail_[my_rank].val += sizeof(val2);
    return result;
  }

  template<typename T>
  std::pair<char *, int> push(const T &value) {
    int my_rank = local::rank_me();
    std::pair<char *, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(value) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char[cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &value, sizeof(T));
    thread_tail_[my_rank].val += sizeof(value);
    return result;
  }

  std::vector<std::pair<char *, int>> flush() {
    //    std::vector<std::pair<int, int>> data;
    //    for (int i = 0; i < thread_num_; ++i) {
    //      data.emplace_back(i, thread_tail_[i]);
    //    }
    //    std::sort(data.begin(), data.end(),
    //              [](std::pair<int, int> a, std::pair<int, int> b) {
    //                return (std::get<1>(a) < std::get<1>(b));
    //              });
    int my_rank = local::rank_me();
    std::vector<std::pair<char *, int>> results;
    if (thread_tail_[my_rank].val > 0) {
      results.emplace_back(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char[cap_];
      thread_tail_[my_rank].val = 0;
    }
    return results;
  }

  private:
  struct AlignedCharPtr {
    alignas(alignof_cacheline) char *val;
  };
  alignas(alignof_cacheline) AlignedCharPtr *thread_ptr_;
  struct AlignedInt {
    alignas(alignof_cacheline) int val;
  };
  alignas(alignof_cacheline) AlignedInt *thread_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) int thread_num_;
};// AggBufferLocal

// A process-level aggregation buffer, light contention between threads
// Each thread has its own memory chunk.
// Each memory chunk has an associated mutex lock.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() will flush all the memory chunks, and thus cost more time.
class AggBufferAdvanced {
  public:
  AggBufferAdvanced() : cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr), lock_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete[] lock_ptr_;
    lock_ptr_ = new AlignedMutex[thread_num_];

    delete[] thread_tail_;
    thread_tail_ = new AlignedInt[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i].val = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete[] thread_ptr_[i].val;
      }
      delete[] thread_ptr_;
    }
    thread_ptr_ = new AlignedCharPtr[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i].val = new char[cap];
    }
  }

  ~AggBufferAdvanced() {
    delete[] lock_ptr_;
    delete[] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete[] thread_ptr_[i].val;
      }
      delete[] thread_ptr_;
    }
  }

  template<typename T, typename U>
  std::pair<char *, int> push(const T &val1, const U &val2) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].val.try_lock()) {
      do_something();
    }
    std::pair<char *, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char[cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val1, sizeof(val1));
    thread_tail_[my_rank].val += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &val2, sizeof(val2));
    thread_tail_[my_rank].val += sizeof(val2);
    lock_ptr_[my_rank].val.unlock();
    return result;
  }

  template<typename T>
  std::pair<char *, int> push(const T &value) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].val.try_lock()) {
      do_something();
    }
    std::pair<char *, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + sizeof(value) > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char[cap_];
      thread_tail_[my_rank].val = 0;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, &value, sizeof(T));
    thread_tail_[my_rank].val += sizeof(value);
    lock_ptr_[my_rank].val.unlock();
    return result;
  }

  std::vector<std::pair<char *, int>> flush() {
    //    std::vector<std::pair<int, int>> data;
    //    for (int i = 0; i < thread_num_; ++i) {
    //      data.emplace_back(i, thread_tail_[i]);
    //    }
    //    std::sort(data.begin(), data.end(),
    //              [](std::pair<int, int> a, std::pair<int, int> b) {
    //                return (std::get<1>(a) < std::get<1>(b));
    //              });
    int my_rank = local::rank_me();
    std::vector<std::pair<char *, int>> results;
    for (int ii = 0; ii < thread_num_; ++ii) {
      int i = (ii + my_rank) % thread_num_;
      if (thread_tail_[i].val == 0) continue;
      while (!lock_ptr_[i].val.try_lock()) {
        do_something();
        if (thread_tail_[i].val == 0) continue;
      }
      if (thread_tail_[i].val > 0) {
        results.emplace_back(thread_ptr_[i].val, thread_tail_[i].val);
        thread_ptr_[i].val = new char[cap_];
        thread_tail_[i].val = 0;
      }
      lock_ptr_[i].val.unlock();
    }
    return results;
  }

  private:
  struct AlignedCharPtr {
    alignas(alignof_cacheline) char *val;
  };
  alignas(alignof_cacheline) AlignedCharPtr *thread_ptr_;
  struct AlignedInt {
    alignas(alignof_cacheline) int val;
  };
  alignas(alignof_cacheline) AlignedInt *thread_tail_;
  alignas(alignof_cacheline) int cap_;
  alignas(alignof_cacheline) int thread_num_;
  struct AlignedMutex {
    alignas(alignof_cacheline) std::mutex val;
  };
  alignas(alignof_cacheline) AlignedMutex *lock_ptr_;
};// class AggBufferAdvanced

// A process-level aggregation buffer, light contention between threads
// All thread shares one memory chunk.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() will flush the memory chunks, and thus cost more time.
// The thread safety between push() and push()/flush() is achieved by atomic operations.
// The thread safety between flush() and flush() is achieved by a mutex lock.
class AggBufferAtomic {
  public:
  using size_type = int64_t;
  AggBufferAtomic() : cap_(0), prefix_(0), tail_(0), reserved_tail_(0), ptr_(nullptr) {}

  ~AggBufferAtomic() {
    delete[] ptr_;
  }

  void init(size_type cap, size_type prefix = 0) {
    ARL_Assert(cap > prefix);
    //    cap_ = cap / sizeof(T) * sizeof(T);
    cap_ = cap;
    prefix_ = prefix;
    delete[] ptr_;
    ptr_ = new char[cap_];
    reserved_tail_ = prefix_;
    tail_ = prefix_;
  }

  template<typename T, typename U>
  std::pair<char *, size_type> push(const T &val1, const U &val2) {
    //    static_assert(std::is_trivially_copyable<T>::value);
    size_type val_size = sizeof(val1) + sizeof(val2);
    std::pair<char *, size_type> result(nullptr, 0);
    size_type current_tail = tail_.fetch_add(val_size);
    while (current_tail > cap_) {
      do_something();
      current_tail = tail_.fetch_add(val_size);
      ARL_Assert(current_tail >= prefix_, "AggBuffer: tail overflow!");
    }
    if (current_tail <= cap_ && current_tail + val_size > cap_) {
      while (!mutex_pop_.try_lock()) {
        do_something();
      }

      while (reserved_tail_ != current_tail) {
        do_something();
      }
      result = std::make_pair(ptr_, current_tail);
      ptr_ = new char[cap_];
      reserved_tail_ = prefix_;
      tail_ = prefix_ + val_size;

      mutex_pop_.unlock();
      current_tail = prefix_;
    }
    std::memcpy(ptr_ + current_tail, &val1, sizeof(val1));
    current_tail += sizeof(val1);
    std::memcpy(ptr_ + current_tail, &val2, sizeof(val2));
    reserved_tail_.fetch_add(val_size);
    return result;
  }

  template<typename T>
  std::pair<char *, size_type> push(const T &val) {
    //    static_assert(std::is_trivially_copyable<T>::value);

    std::pair<char *, size_type> result(nullptr, 0);
    size_type current_tail = tail_.fetch_add(sizeof(val));
    while (current_tail > cap_) {
      do_something();
      current_tail = tail_.fetch_add(sizeof(val));
      ARL_Assert(current_tail >= prefix_, "AggBuffer: tail overflow!");
    }
    if (current_tail <= cap_ && current_tail + sizeof(val) > cap_) {
      while (!mutex_pop_.try_lock()) {
        do_something();
      }

      while (reserved_tail_ != current_tail) {
        do_something();
      }
      result = std::make_pair(ptr_, current_tail);
      ptr_ = new char[cap_];
      reserved_tail_ = prefix_;
      tail_ = prefix_ + sizeof(val);

      mutex_pop_.unlock();
      current_tail = prefix_;
    }
    std::memcpy(ptr_ + current_tail, &val, sizeof(val));
    reserved_tail_.fetch_add(sizeof(val));
    return result;
  }

  std::vector<std::pair<char *, size_type>> flush() {
    std::vector<std::pair<char *, size_type>> result;
    if (tail_.load() == prefix_) {
      return result;
    }
    if (!mutex_pop_.try_lock()) {
      return result;
    }
    size_type current_tail = tail_.fetch_add(cap_ + 1);// prevent others from begining pushing
    if (current_tail <= cap_ && current_tail > prefix_) {
      // wait until those who is pushing finish
      while (reserved_tail_ != current_tail) {
        do_something();
        //        printf("rank %ld wait for flush %d, %d\n", rank_me(), reserved_tail_.load(), current_tail);
      }

      result.emplace_back(ptr_, current_tail);
      ptr_ = new char[cap_];
      reserved_tail_ = prefix_;
      tail_ = prefix_;
    } else if (current_tail == prefix_) {
      tail_ = prefix_;
    }// else (current_tail > cap_): some push will flush the buffer
    mutex_pop_.unlock();
    return result;
  }

  [[nodiscard]] size_type get_size() const {
    return tail_.load();
  }

  [[nodiscard]] std::string get_status() const {
    std::ostringstream os;
    os << "ptr_  = " << (void *) ptr_ << '\n';
    os << "tail_ = " << tail_.load() << "\n";
    os << "reserved_tail_ = " << reserved_tail_.load() << "\n";
    os << "cap_  = " << cap_ << "\n";
    return os.str();
  }

  private:
  alignas(alignof_cacheline) char *ptr_;
  alignas(alignof_cacheline) std::atomic<size_type> tail_;
  alignas(alignof_cacheline) std::atomic<size_type> reserved_tail_;
  alignas(alignof_cacheline) size_type cap_;
  alignas(alignof_cacheline) size_type prefix_;
  alignas(alignof_cacheline) std::mutex mutex_pop_;
};// class AggBufferAtomic

//using AggBuffer = AggBufferSimple;
//using AggBuffer = AggBufferLocal;
//using AggBuffer = AggBufferAdvanced;
using AggBuffer = AggBufferAtomic;

}// namespace am_internal
}// namespace arl

#endif//ARL_AGG_BUFFER_HPP
