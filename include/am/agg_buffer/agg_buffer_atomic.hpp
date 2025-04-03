#ifndef ARL_AGG_BUFFER_ATOMIC_HPP
#define ARL_AGG_BUFFER_ATOMIC_HPP

namespace arl::am_internal {
// A process-level aggregation buffer, light contention between threads
// All thread shares one memory chunk.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() will flush the memory chunks, and thus cost more time.
// The thread safety between push() and push()/flush() is achieved by atomic operations.
// The thread safety between flush() and flush() is achieved by a mutex lock.
class AggBufferAtomic : public AggBuffer {
  public:
  using size_type = int64_t;
  AggBufferAtomic() : cap_(0), prefix_(0), tail_(0), reserved_tail_(0), ptr_(nullptr) {}

  ~AggBufferAtomic() override {
    backend::buffer_free(ptr_);
  }

  void init(size_type cap, size_type prefix) override {
    ARL_Assert(cap > prefix);
    //    cap_ = cap / sizeof(T) * sizeof(T);
    cap_ = cap;
    prefix_ = prefix;
    backend::buffer_free(ptr_);
    ptr_ = (char *) backend::buffer_alloc(cap_);
    reserved_tail_ = prefix_;
    tail_ = prefix_;
  }

  std::pair<char *, size_type> push(char *p1, size_t s1,
                                    char *p2, size_t s2) override {
    //    static_assert(std::is_trivially_copyable<T>::value);
    size_type val_size = s1 + s2;
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
      ptr_ = (char *) backend::buffer_alloc(cap_);
      reserved_tail_ = prefix_;
      tail_ = prefix_ + val_size;

      mutex_pop_.unlock();
      current_tail = prefix_;
    }
    std::memcpy(ptr_ + current_tail, p1, s1);
    current_tail += s1;
    std::memcpy(ptr_ + current_tail, p2, s2);
    reserved_tail_.fetch_add(val_size);
    return result;
  }

  std::pair<char *, size_type> push(char *p, size_t s) override {
    //    static_assert(std::is_trivially_copyable<T>::value);

    std::pair<char *, size_type> result(nullptr, 0);
    size_type current_tail = tail_.fetch_add(s);
    while (current_tail > cap_) {
      do_something();
      current_tail = tail_.fetch_add(s);
      ARL_Assert(current_tail >= prefix_, "AggBuffer: tail overflow!");
    }
    if (current_tail <= cap_ && current_tail + s > cap_) {
      while (!mutex_pop_.try_lock()) {
        do_something();
      }

      while (reserved_tail_ != current_tail) {
        do_something();
      }
      result = std::make_pair(ptr_, current_tail);
      ptr_ = (char *) backend::buffer_alloc(cap_);
      reserved_tail_ = prefix_;
      tail_ = prefix_ + s;

      mutex_pop_.unlock();
      current_tail = prefix_;
    }
    std::memcpy(ptr_ + current_tail, p, s);
    reserved_tail_.fetch_add(s);
    return result;
  }

  std::pair<char *, size_type> reserve(size_t s, char** p) {
    std::pair<char *, size_type> result(nullptr, 0);
    size_type current_tail = tail_.fetch_add(s);
    while (current_tail > cap_) {
      do_something();
      current_tail = tail_.fetch_add(s);
      ARL_Assert(current_tail >= prefix_, "AggBuffer: tail overflow!");
    }
    if (current_tail <= cap_ && current_tail + s > cap_) {
      while (!mutex_pop_.try_lock()) {
        do_something();
      }

      while (reserved_tail_ != current_tail) {
        do_something();
      }
      result = std::make_pair(ptr_, current_tail);
      ptr_ = (char *) backend::buffer_alloc(cap_);
      reserved_tail_ = prefix_;
      tail_ = prefix_ + s;

      mutex_pop_.unlock();
      current_tail = prefix_;
    }
    *p = ptr_ + current_tail;
    return result;
  }

  void commit(size_t s) {
    reserved_tail_.fetch_add(s);
  }

  std::vector<std::pair<char *, size_type>> flush() override {
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
      ptr_ = (char *) backend::buffer_alloc(cap_);
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

}// namespace arl::am_internal

#endif//ARL_AGG_BUFFER_ATOMIC_HPP
