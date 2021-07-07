#ifndef ARL_THREADBARRIER_HPP
#define ARL_THREADBARRIER_HPP

namespace arl {
class ThreadBarrier {
 public:
  void init(size_t thread_num) {
    ARL_Assert(thread_num > 0, "Error: thread_num cannot be 0.");
    thread_num_ = thread_num;
  }

  void wait() {
    ARL_Assert(thread_num_ > 0, "Error: call wait() before init().");
    size_t mstep = step.load();

    if (++waiting == thread_num_) {
      waiting = 0;
      step++;
    }
    else {
      progress_external_until([&](){return step != mstep;});
    }
  }

 private:
  alignas(alignof_cacheline) std::atomic<size_t> waiting = 0;
  alignas(alignof_cacheline) std::atomic<size_t> step = 0;
  alignas(alignof_cacheline) size_t thread_num_ = 0;
};
} // namespace arl

#endif //ARL_THREADBARRIER_HPP
