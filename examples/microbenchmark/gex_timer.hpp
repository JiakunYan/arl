//
// Created by Jiakun Yan on 11/12/19.
//
#ifndef ARL_TIMER_HPP
#define ARL_TIMER_HPP

#include <string>
#include <gasnetex.h>

namespace arl {
using tick_t = int64_t;
  // microseconds, 12ns
  tick_t ticks_now() {
    return gasneti_ticks_now();
  }

  uint64_t ticks_to_ns(tick_t val) {
    return gasneti_ticks_to_ns(val);
  }

  uint64_t ticks_to_us(tick_t val) {
    return ticks_to_ns(val) / 1e3;
  }

  double ticks_to_ms(tick_t val) {
    return ticks_to_ns(val) / 1e6;
  }

  double ticks_to_s(tick_t val) {
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
//      if (rank_me() == 0) {
//        std::printf("val=%lu; num=%lu; average=%.2lf\n", val, num, average);
//      }
  }

  struct SimpleTimer {
  private:
    unsigned long step = 0;
    tick_t _start = 0;
    double _ticks = 0;
  public:
    void start() {
      _start = ticks_now();
    }

    void end() {
      tick_t _end = ticks_now();
      update_average(_ticks, _end - _start, ++step);
    }

    void tick_and_update(tick_t _start_) {
      tick_t _end = ticks_now();
      update_average(_ticks, _end - _start_, ++step);
    }

    [[nodiscard]] double to_ns() const {
      return ticks_to_ns(_ticks);
    }

    [[nodiscard]] double to_us() const {
      return ticks_to_ns(_ticks) / 1e3;
    }

    [[nodiscard]] double to_s() const {
      return ticks_to_ns(_ticks) / 1e9;
    }

    void print_us(std::string &&name = "") const {
      printf("Duration %s: %.3lf us\n", name.c_str(), to_us());
    }
  };
}
#endif //ARL_TIMER_HPP
