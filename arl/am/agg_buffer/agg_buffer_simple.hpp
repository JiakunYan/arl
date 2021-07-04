#ifndef ARL_AGG_BUFFER_SIMPLE_HPP
#define ARL_AGG_BUFFER_SIMPLE_HPP

namespace arl::am_internal {
// A simple aggregation buffer, slow but should hardly have bugs.
// All the worker threads share one memory chunk.
// Use one mutex lock to ensure its thread-safety.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() will trivially flush the whole aggregation buffer, because the buffer only has one memory chunk.
class AggBufferSimple : public AggBuffer {
  public:
  using size_type = int64_t;
  AggBufferSimple() : cap_(0), tail_(0), ptr_(nullptr), prefix_(0) {
  }

  void init(size_type cap, size_type prefix) override {
    ARL_Assert(cap > 0);
    cap_ = cap;
    prefix_ = prefix;
    tail_ = prefix_;
    delete[] ptr_;
    ptr_ = new char[cap];
  }

  ~AggBufferSimple() override {
    delete[] ptr_;
  }

  std::pair<char *, size_type> push(char *p1, size_t s1,
                                    char *p2, size_t s2) override {
    while (!lock.try_lock()) {
      do_something();
    }
    std::pair<char *, size_type> result(nullptr, 0);
    if (tail_ + s1 + s2 > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = prefix_;
    }
    std::memcpy(ptr_ + tail_, p1, s1);
    tail_ += s1;
    std::memcpy(ptr_ + tail_, p2, s2);
    tail_ += s2;
    lock.unlock();
    return result;
  }

  std::pair<char *, size_type> push(char* p, size_t s) override {
    while (!lock.try_lock()) {
      do_something();
    }
    std::pair<char *, size_type> result(nullptr, 0);
    if (tail_ + s > cap_) {
      result = std::make_pair(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = prefix_;
    }
    std::memcpy(ptr_ + tail_, p, s);
    tail_ += s;
    lock.unlock();
    return result;
  }

  std::vector<std::pair<char *, size_type>> flush() override {
    std::vector<std::pair<char *, size_type>> result;
    if (tail_ == prefix_) return result;
    while (!lock.try_lock()) {
      do_something();
      if (tail_ == prefix_) return result;
    }
    if (tail_ > prefix_) {
      result.emplace_back(ptr_, tail_);
      ptr_ = new char[cap_];
      tail_ = prefix_;
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
  alignas(alignof_cacheline) size_type prefix_;
  alignas(alignof_cacheline) std::mutex lock;
};
} // namespace arl::am_internal

#endif//ARL_AGG_BUFFER_SIMPLE_HPP
