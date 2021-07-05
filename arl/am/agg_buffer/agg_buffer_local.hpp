#ifndef ARL_AGG_BUFFER_LOCAL_HPP
#define ARL_AGG_BUFFER_LOCAL_HPP

namespace arl::am_internal {
// A thread-local aggregation buffer, no contention between threads.
// Each thread has its own memory chunk.
// push() will push one or two elements into the buffer and, if the buffer is full, flush the buffer.
// flush() only flushes the caller's local memory chunk.
class AggBufferLocal : public AggBuffer {
  public:
  using size_type = int64_t;
  AggBufferLocal(): cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr), prefix_(0) {
  }

  void init(size_type cap, size_type prefix) override {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = local::rank_n();
    prefix_ = prefix;

    delete [] thread_tail_;
    thread_tail_ = new AlignedInt [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i].val = prefix_;
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

  ~AggBufferLocal() override {
    delete [] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i].val;
      }
      delete [] thread_ptr_;
    }
  }

  std::pair<char *, size_type> push(char *p1, size_t s1,
                                    char *p2, size_t s2) override {
    int my_rank = local::rank_me();
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + s1 + s2 > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = prefix_;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, p1, s1);
    thread_tail_[my_rank].val += s1;
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, p2, s2);
    thread_tail_[my_rank].val += s2;
    return result;
  }

  std::pair<char *, size_type> push(char* p, size_t s) override {
    int my_rank = local::rank_me();
    std::pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank].val + s > cap_) {
      // push my
      result = std::make_pair(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = prefix_;
    }
    std::memcpy(thread_ptr_[my_rank].val + thread_tail_[my_rank].val, p, s);
    thread_tail_[my_rank].val += s;
    return result;
  }

  std::vector<std::pair<char *, size_type>> flush() override {
//    std::vector<std::pair<int, int>> data;
//    for (int i = 0; i < thread_num_; ++i) {
//      data.emplace_back(i, thread_tail_[i]);
//    }
//    std::sort(data.begin(), data.end(),
//              [](std::pair<int, int> a, std::pair<int, int> b) {
//                return (std::get<1>(a) < std::get<1>(b));
//              });
    int my_rank = local::rank_me();
    std::vector<std::pair<char*, size_type>> results;
    if (thread_tail_[my_rank].val > prefix_) {
      results.emplace_back(thread_ptr_[my_rank].val, thread_tail_[my_rank].val);
      thread_ptr_[my_rank].val = new char [cap_];
      thread_tail_[my_rank].val = prefix_;
    }
    return results;
  }

  [[nodiscard]] size_type get_size() const {
    size_type total = 0;
    for (int i = 0; i < thread_num_; ++i) {
      total += thread_tail_[i].val;
    }
    return total;
  }

  [[nodiscard]] std::string get_status() const {
    std::ostringstream os;
    for (int i = 0; i < thread_num_; ++i) {
      os << "BUFFER[" << i << "]:\n";
      os << "ptr_  = " << (void *) thread_ptr_[i].val << '\n';
      os << "tail_ = " << thread_tail_[i].val << "\n";
      os << "cap_  = " << cap_ << "\n";
    }
    return os.str();
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
  alignas(alignof_cacheline) size_type cap_;
  alignas(alignof_cacheline) int thread_num_;
  alignas(alignof_cacheline) size_type prefix_;
}; // AggBufferLocal
} // namespace arl::am_internal

#endif//ARL_AGG_BUFFER_LOCAL_HPP
