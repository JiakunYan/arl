//
// Created by Jiakun Yan on 10/1/19.
//

#ifndef ARL_BASE_HPP
#define ARL_BASE_HPP

namespace arl {
namespace am_internal {
extern void init_am();
extern void exit_am();
extern void init_am_thread();
extern void exit_am_thread();
}// namespace am_internal

extern std::atomic<bool> thread_run;
extern std::atomic<bool> worker_exit;

extern intptr_t base_fnptr;

extern void init(size_t custom_num_workers_per_proc = 15,
          size_t custom_num_threads_per_proc = 16);

extern void finalize();

// progress thread
static void progress_handler(rank_t id) {
  rank_internal::my_rank = id;
  rank_internal::my_context = id;
  while (!thread_run) {
    usleep(1);
  }
  while (!worker_exit) {
    progress_internal();
  }
}

template<typename Fn, typename... Args>
static void worker_handler(Fn &&fn, rank_t id, Args &&...args) {
  rank_internal::my_rank = id;
  rank_internal::my_context = id;
  while (!thread_run) {
    usleep(1);
  }
  barrier();
  std::invoke(std::forward<Fn>(fn),
              std::forward<Args>(args)...);
  barrier();
}

static void set_affinity(pthread_t pthread_handler, size_t target) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(target, &cpuset);
  int rv = pthread_setaffinity_np(pthread_handler, sizeof(cpuset), &cpuset);
  if (rv != 0) {
    throw std::runtime_error("ERROR thread affinity didn't work.");
  }
}

template<typename Fn, typename... Args>
static void run(Fn &&fn, Args &&...args) {
  using fn_t = decltype(+std::declval<std::remove_reference_t<Fn>>());
  base_fnptr = reinterpret_cast<intptr_t>(&fn);

  std::vector<std::thread> worker_pool;
  std::vector<std::thread> progress_pool;
  thread_run = false;
  worker_exit = false;

  int numberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);
  int my_cpu = sched_getcpu();
  size_t cpuoffset = my_cpu / local::thread_n() * local::thread_n();
  ARL_LOG(DEBUG, "Rank %ld Number of processors: %d; my_cpu: %d; cpuoffset %lu\n", proc::rank_me(), numberOfProcessors, my_cpu, cpuoffset);
  // if ((my_cpu >= 0 && my_cpu < 16) || (my_cpu >= 32 && my_cpu < 48)) {
  // cpuoffset = 0;
  // } else {
  // cpuoffset = 16;
  // }

  int num_progress_thread = local::thread_n() - local::rank_n();

  if (num_progress_thread > 0) {
    int workers_per_progress = (local::rank_n() + num_progress_thread - 1) / num_progress_thread;
    // interleave worker threads and progress threads
    size_t cpu_idx = 0;
    for (size_t i = 0; i < num_progress_thread; ++i) {
      for (size_t j = 0; j < workers_per_progress; ++j) {
        size_t idx = i * workers_per_progress + j;
        if (idx >= local::rank_n()) {
          break;
        }
        std::thread t(worker_handler<fn_t, std::remove_reference_t<Args>...>,
                      std::forward<fn_t>(+fn), idx,
                      std::forward<Args>(args)...);
        if (config::pin_thread) {
          size_t final_cpu_idx = (cpu_idx++ + cpuoffset) % numberOfProcessors;
          set_affinity(t.native_handle(), final_cpu_idx);
          ARL_LOG(DEBUG, "Rank %ld worker thread %lu is pinned to CPU %lu\n", proc::rank_me(), worker_pool.size(), final_cpu_idx);
        }
        worker_pool.push_back(std::move(t));
      }
      std::thread t(progress_handler, local::rank_n() + i);
      if (config::pin_thread) {
        size_t final_cpu_idx = (cpu_idx++ + cpuoffset) % numberOfProcessors;
        set_affinity(t.native_handle(), final_cpu_idx);
        ARL_LOG(DEBUG, "Rank %ld progress thread %lu is pinned to CPU %lu\n", proc::rank_me(), progress_pool.size(), final_cpu_idx);
      }
      progress_pool.push_back(std::move(t));
    }
  } else {
    for (size_t i = 0; i < local::rank_n(); ++i) {
      std::thread t(worker_handler<fn_t, std::remove_reference_t<Args>...>,
                    std::forward<fn_t>(+fn), i,
                    std::forward<Args>(args)...);
      if (config::pin_thread) {
        size_t final_cpu_idx = (i + cpuoffset) % numberOfProcessors;
        set_affinity(t.native_handle(), final_cpu_idx);
        ARL_LOG(DEBUG, "Rank %ld worker thread %lu is pinned to CPU %lu\n", proc::rank_me(), worker_pool.size(), final_cpu_idx);
      }
      worker_pool.push_back(std::move(t));
    }
  }

  //   for (size_t i = 0; i < local::rank_n(); ++i) {
  //     std::thread t(worker_handler<fn_t, std::remove_reference_t<Args>...>,
  //                   std::forward<fn_t>(+fn), i,
  //                   std::forward<Args>(args)...);
  // #ifdef ARL_THREAD_PIN
  //     set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
  // #endif
  //     worker_pool.push_back(std::move(t));
  //   }

  //   for (size_t i = local::rank_n(); i < local::thread_n(); ++i) {
  //     std::thread t(progress_handler, i - local::rank_n());
  // #ifdef ARL_THREAD_PIN
  //     set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
  // #endif
  //     progress_pool.push_back(std::move(t));
  //   }
  thread_run = true;

  for (size_t i = 0; i < local::rank_n(); ++i) {
    worker_pool[i].join();
  }

  worker_exit = true;

  for (size_t i = 0; i < local::thread_n() - local::rank_n(); ++i) {
    progress_pool[i].join();
  }
}

template<typename... Args>
static void print(const std::string &format, Args... args) {
  fflush(stdout);
  pure_barrier();
  if (rank_me() == 0) {
    if constexpr (sizeof...(args) == 0) {
      printf("%s", format.c_str());
    } else {
      printf(format.c_str(), args...);
    }
  }
  fflush(stdout);
  pure_barrier();
}

namespace proc {
template<typename... Args>
static void print(const std::string &format, Args... args) {
  fflush(stdout);
  proc::barrier();
  if (proc::rank_me() == 0) {
    if constexpr (sizeof...(args) == 0) {
      printf("%s", format.c_str());
    } else {
      printf(format.c_str(), args...);
    }
  }
  fflush(stdout);
  proc::barrier();
}
}// namespace proc

template<typename... Args>
static void cout(Args... args) {
  std::cout.flush();
  barrier();
  if (rank_me() == 0) {
    os_print(std::cout, args...);
  }
  std::cout.flush();
  barrier();
}
}// namespace arl

#endif//ARL_BASE_HPP
