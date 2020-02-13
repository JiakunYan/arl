//
// Created by Jiakun Yan on 10/1/19.
//

#ifndef ARL_BASE_HPP
#define ARL_BASE_HPP

namespace arl {
  extern void init_am(void);
  extern void flush_am(void);
  extern void init_agg(void);
  extern void flush_agg_buffer(void);
  extern void progress(void);

  alignas(alignof_cacheline) ThreadBarrier threadBarrier;
  alignas(alignof_cacheline) std::atomic<bool> thread_run = false;
  alignas(alignof_cacheline) std::atomic<bool> worker_exit = false;

  inline void barrier() {
    threadBarrier.wait();
    if (local::rank_me() == 0) {
      flush_agg_buffer();
    }
    flush_am();
    if (local::rank_me() == 0) {
      backend::barrier();
    }
    threadBarrier.wait();
  }

  // progress thread
  void progress_handler() {
    while (!thread_run) {
      usleep(1);
    }
    while (!worker_exit) {
      progress();
    }
  }

  template <typename Fn, typename... Args>
  void worker_handler(Fn &&fn, Args &&... args) {
    while (!thread_run) {
      usleep(1);
    }
    barrier();
    std::invoke(std::forward<Fn>(fn),
                std::forward<Args>(args)...);
    barrier();
  }

  inline void init(size_t custom_num_workers_per_proc = 15,
      size_t custom_num_threads_per_proc = 16, size_t shared_segment_size = 256) {
    backend::init(shared_segment_size, true);

#ifdef ARL_DEBUG
    backend::print("%s", "WARNING: Running low-performance debug mode.\n");
#endif
#ifndef ARL_THREAD_PIN
    backend::print("%s", "WARNING: Haven't pinned threads to cores.\n");
#endif
    num_workers_per_proc = custom_num_workers_per_proc;
    num_threads_per_proc = custom_num_threads_per_proc;
    thread_contexts.resize(num_threads_per_proc);
    threadBarrier.init(num_workers_per_proc, progress);

    init_am();
    init_agg();
  }

  inline void finalize() {
    backend::finalize();
  }

  void set_affinity(pthread_t pthread_handler, size_t target) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(target, &cpuset);
    int rv = pthread_setaffinity_np(pthread_handler, sizeof(cpuset), &cpuset);
    if (rv != 0) {
      throw std::runtime_error("ERROR thread affinity didn't work.");
    }
  }

  template <typename Fn, typename... Args>
  void run(Fn &&fn, Args &&... args) {
    using fn_t = decltype(+std::declval<std::remove_reference_t<Fn>>());
    std::vector<std::thread> worker_pool;
    std::vector<std::thread> progress_pool;

#ifdef ARL_THREAD_PIN
    int numberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);
    size_t cpuoffset;
    int my_cpu = sched_getcpu();
    if ((my_cpu >= 0 && my_cpu < 16) || (my_cpu >= 32 && my_cpu < 48)) {
      cpuoffset = 0;
    } else {
      cpuoffset = 16;
    }
#endif

    for (size_t i = 0; i < num_workers_per_proc; ++i) {
      std::thread t(worker_handler<fn_t, std::remove_reference_t<Args>...>,
                    std::forward<fn_t>(+fn),
                    std::forward<Args>(args)...);
#ifdef ARL_THREAD_PIN
      set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
#endif
      thread_ids[t.get_id()] = i;
      thread_contexts[i].val = i;
      worker_pool.push_back(std::move(t));
    }

    for (size_t i = num_workers_per_proc; i < num_threads_per_proc; ++i) {
      std::thread t(progress_handler);
#ifdef ARL_THREAD_PIN
      set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
#endif
      thread_ids[t.get_id()] = i;
      thread_contexts[i].val = i;
      progress_pool.push_back(std::move(t));
    }
    thread_run = true;

    for (size_t i = 0; i < num_workers_per_proc; ++i) {
      worker_pool[i].join();
    }

    worker_exit = true;

    for (size_t i = 0; i < num_threads_per_proc - num_workers_per_proc; ++i) {
      progress_pool[i].join();
    }
  }

  template <typename ...Args>
  void print(std::string format, Args... args) {
    fflush(stdout);
    barrier();
    if (rank_me() == 0) {
      if constexpr (sizeof...(args) == 0) {
        printf("%s", format.c_str());
      } else {
        printf(format.c_str(), args...);
      }
    }
    fflush(stdout);
    barrier();
  }

  template <typename ...Args>
  void cout(Args... args) {
    std::cout.flush();
    barrier();
    if (rank_me() == 0) {
      os_print(std::cout, args...);
    }
    std::cout.flush();
    barrier();
  }

  namespace local {
    inline void barrier() {
      threadBarrier.wait();
    }
  }

  namespace proc {
    inline void barrier() {
      backend::barrier();
    }
  }
}

#endif //ARL_BASE_HPP
