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

static void init(size_t custom_num_workers_per_proc = 15,
          size_t custom_num_threads_per_proc = 16) {
  config::init();
  backend::init();

#ifdef ARL_DEBUG
  proc::print("%s", "WARNING: Running low-performance debug mode.\n");
#endif
#ifndef ARL_THREAD_PIN
  proc::print("%s", "WARNING: Haven't pinned threads to cores.\n");
#endif
  rank_internal::num_workers_per_proc = custom_num_workers_per_proc;
  rank_internal::num_threads_per_proc = custom_num_threads_per_proc;
  threadBarrier.init(local::rank_n());

  am_internal::init_am();
}

static void finalize() {
  am_internal::exit_am();
  backend::finalize();
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
  std::vector<std::thread> worker_pool;
  std::vector<std::thread> progress_pool;
  thread_run = false;
  worker_exit = false;

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

  for (size_t i = 0; i < local::rank_n(); ++i) {
    std::thread t(worker_handler<fn_t, std::remove_reference_t<Args>...>,
                  std::forward<fn_t>(+fn), i,
                  std::forward<Args>(args)...);
#ifdef ARL_THREAD_PIN
    set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
#endif
    worker_pool.push_back(std::move(t));
  }

  for (size_t i = local::rank_n(); i < local::thread_n(); ++i) {
    std::thread t(progress_handler, i - local::rank_n());
#ifdef ARL_THREAD_PIN
    set_affinity(t.native_handle(), (i + cpuoffset) % numberOfProcessors);
#endif
    progress_pool.push_back(std::move(t));
  }
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
