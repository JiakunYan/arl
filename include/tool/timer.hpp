#ifndef ARL_TIMER_HPP
#define ARL_TIMER_HPP

namespace arl {
template<typename T, typename BinaryOp>
extern T reduce_all(const T &value, const BinaryOp &op);

// microseconds, 12ns
static inline tick_t ticks_now() {
  tick_t ret;
  struct timeval t1;
  gettimeofday(&t1, 0);
  ret = t1.tv_sec * 1e9 + t1.tv_usec * 1e3;
  return ret;
}

static inline uint64_t ticks_to_ns(tick_t val) {
  uint64_t ret;
  ret = val;
  return ret;
}

static inline uint64_t ticks_to_us(tick_t val) {
  return ticks_to_ns(val) / 1e3;
}

static inline double ticks_to_ms(tick_t val) {
  return ticks_to_ns(val) / 1e6;
}

static inline double ticks_to_s(tick_t val) {
  return ticks_to_ns(val) / 1e9;
}

// microseconds, 370ns
//  tick_t ticks_now() {
//    timespec temp;
//    clock_gettime(CLOCK_MONOTONIC, &temp);
//    return temp.tv_sec * 1e6 + temp.tv_nsec / 1e3;
//  }

inline void update_average(double &average, uint64_t val, uint64_t num) {
  average += (val - average) / num;
}

static inline void usleep(int64_t utime) {
  tick_t start_tick = ticks_now();
  while (ticks_to_us(ticks_now() - start_tick) < utime) continue;
}

struct SimpleTimer {
  public:
  void start() {
    _start = ticks_now();
  }

  void end() {
    tick_t _end = ticks_now();
    update_average(_ticks, _end - _start, ++_step);
  }

  void tick_and_update(tick_t _start_) {
    tick_t _end = ticks_now();
    update_average(_ticks, _end - _start_, ++_step);
  }

  [[nodiscard]] double to_ns() const {
    return ticks_to_ns(_ticks);
  }

  [[nodiscard]] double to_us() const {
    return ticks_to_ns(_ticks) / 1e3;
  }

  [[nodiscard]] double to_ms() const {
    return ticks_to_ns(_ticks) / 1e6;
  }

  [[nodiscard]] double to_s() const {
    return ticks_to_ns(_ticks) / 1e9;
  }

  void print_us(std::string &&name = "") const {
    printf("Duration %s: %.3lf us (step %ld, total %.3lf s)\n", name.c_str(), to_us(), step(), to_s() * step());
  }

  void print_ms(std::string &&name = "") const {
    printf("Duration %s: %.3lf ms (step %ld, total %.3lf s)\n", name.c_str(), to_ms(), step(), to_s() * step());
  }

  void print_s(std::string &&name = "") const {
    printf("Duration %s: %.3lf s (step %ld, total %.3lf s)\n", name.c_str(), to_s(), step(), to_s() * step());
  }

  SimpleTimer &col_print_us(std::string &&name = "", rank_t r = 0) {
    double max_duration = reduce_all(to_us(), op_max());
    double ave_duration = reduce_all(to_us(), op_plus()) / rank_n();
    double min_duration = reduce_all(to_us(), op_min());
    int64_t max_step = reduce_all(_step, op_max());
    int64_t ave_step = reduce_all(_step, op_plus()) / rank_n();
    int64_t min_step = reduce_all(_step, op_min());
    double max_total_duration = reduce_all(to_s() * step(), op_max());
    double ave_total_duration = reduce_all(to_s() * step(), op_plus()) / rank_n();
    double min_total_duration = reduce_all(to_s() * step(), op_min());
    if (rank_me() == r)
      printf("Duration %s: ave(min/max) one %.3f(%.3f,%.3f) us step %ld(%ld,%ld) total %.3f(%.3f,%.3f) s\n",
             name.c_str(),
             ave_duration, min_duration, max_duration,
             ave_step, min_step, max_step,
             ave_total_duration, min_total_duration, max_total_duration);
    return *this;
  }

  SimpleTimer &col_print_s(std::string &&name = "", rank_t r = 0) {
    double max_duration = reduce_all(to_s(), op_max());
    double ave_duration = reduce_all(to_s(), op_plus()) / rank_n();
    double min_duration = reduce_all(to_s(), op_min());
    int64_t max_step = reduce_all(_step, op_max());
    int64_t ave_step = reduce_all(_step, op_plus()) / rank_n();
    int64_t min_step = reduce_all(_step, op_min());
    double max_total_duration = reduce_all(to_s() * step(), op_max());
    double ave_total_duration = reduce_all(to_s() * step(), op_plus()) / rank_n();
    double min_total_duration = reduce_all(to_s() * step(), op_min());
    if (rank_me() == r)
      printf("Duration %s: ave(min/max) one %.3f(%.3f,%.3f) s step %ld(%ld,%ld) total %.3f(%.3f,%.3f) s\n",
             name.c_str(),
             ave_duration, min_duration, max_duration,
             ave_step, min_step, max_step,
             ave_total_duration, min_total_duration, max_total_duration);
    return *this;
  }

  long step() const {
    return _step;
  }

  void clear() {
    _step = 0;
    _start = 0;
    _ticks = 0;
  }

  private:
  int64_t _step = 0;
  tick_t _start = 0;
  double _ticks = 0;
};
}// namespace arl
#endif//ARL_TIMER_HPP
