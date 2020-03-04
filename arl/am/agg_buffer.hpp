//
// Created by Jiakun Yan on 10/25/19.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

#include <atomic>
#include <vector>
#include <cstring>

using std::unique_ptr;
using std::make_unique;
using std::vector;

namespace arl {
  class AggBuffer {
    alignas(alignof_cacheline) unique_ptr<char[]> buffer;
    alignas(alignof_cacheline) std::atomic<size_t> tail;
    alignas(alignof_cacheline) std::atomic<size_t> reserved_tail;
    alignas(alignof_cacheline) size_t cap;
    alignas(alignof_cacheline) std::mutex mutex_pop;

  public:
    enum class status_t {
      FAIL,
      SUCCESS,
      SUCCESS_AND_FULL
    };

    AggBuffer(): cap(0), tail(0), reserved_tail(0), buffer(nullptr) {
    }

    explicit AggBuffer(size_t size_): cap(size_), tail(0), reserved_tail(0) {
      buffer = make_unique<char[]>(cap);
    }

    ~AggBuffer() {
    }

    template <typename T>
    void init(size_t size_) {
      cap = size_ / sizeof(T) * sizeof(T);
      tail = 0;
      reserved_tail = 0;
      buffer = make_unique<char[]>(cap);
    }

    size_t size() const {
      return reserved_tail.load();
    }

    template <typename T>
    status_t push(const T& val) {
      static_assert(std::is_trivially_copyable<T>::value);
      size_t current_tail = tail.fetch_add(sizeof(val));
      if (current_tail + sizeof(val) > cap) {
#ifdef ARL_DEBUG
        if (current_tail > std::numeric_limits<size_t>::max() / 4 * 3) {
          throw std::overflow_error("arl::AggBuffer::push: tail might overflow");
        }
#endif
        return status_t::FAIL;
      }
      std::memcpy(buffer.get() + current_tail, &val, sizeof(val));
      size_t temp = reserved_tail.fetch_add(sizeof(val));
//      printf("push real %lu reserved %lu\n", tail.load(), reserved_tail.load());
      if (temp + 2 * sizeof(val) > cap) {
        return status_t::SUCCESS_AND_FULL;
      } else {
        return status_t::SUCCESS;
      }
    }

    size_t pop_all(unique_ptr<char[]>& target) {
      if (tail.load() == 0) {
        return 0;
      }
      if (mutex_pop.try_lock()) {
        size_t real_tail = std::min(tail.fetch_add(cap), cap); // prevent others from begining pushing
        // wait until those who is pushing finish
        while (reserved_tail != real_tail) {
        }

        target = std::move(buffer);
        buffer = make_unique<char[]>(cap);

        ARL_Assert(real_tail <= cap, "");
        ARL_Assert(real_tail == reserved_tail, "");

        reserved_tail = 0;
        tail = 0;

        mutex_pop.unlock();
        return real_tail;
      } else {
        // someone is poping
        return 0;
      }
    }

  };
}

#endif //ARL_AGG_BUFFER_HPP
