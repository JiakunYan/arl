//
// Created by jackyan on 3/18/20.
//

#ifndef ARL_FUTURE_HPP
#define ARL_FUTURE_HPP

namespace arl {
namespace am_internal {

template<typename T>
class FutureData {
 public:
  FutureData(): ready_(false) {}
  bool ready() {
    return ready_.load();
  }
  void load(const T& val) {
    if (ready_.load()) {
      throw std::runtime_error(string_format("load ", this, " but already ready!"));
    }
    data_ = val;
    ready_ = true;
  }
  T get() {
    if (ready_.load()) {
      return data_;
    } else {
      throw std::runtime_error("get without ready!");
    }
  }
//  void set_ready() {
//    ready_ = true;
//  }
 private:
  std::atomic<bool> ready_;
  T data_;
};

template<>
class FutureData<void> {
 public:
  FutureData(): ready_(false) {}
  bool ready() {
    return ready_.load();
  }
  void load() {
    if (ready_.load()) {
      throw std::runtime_error(string_format("load ", this, " but already ready!"));
    }
    ready_ = true;
  }
  void get() {
    if (ready_.load()) {
      return;
    } else {
      throw std::runtime_error("get without ready!");
    }
  }

//  void set_ready() {
//    ready_ = true;
//  }
 private:
  std::atomic<bool> ready_;
};
} // namespace am_internal

template<typename T>
class Future {
 public:
  Future() : data_p_(new am_internal::FutureData<T>()) {}
  Future(const Future& future) = delete;
  Future& operator=(const Future& future) = delete;
  Future(Future&& future) noexcept = default;
  Future& operator=(Future&& future) noexcept = default;

  ~Future() {
    if (data_p_ != nullptr) {
      while (!data_p_->ready()) {
        progress();
      }
    }
  }

  intptr_t get_p() const {
    return reinterpret_cast<intptr_t>(data_p_.get());
  }

  T get() const {
    while (!data_p_->ready()) {
      progress();
    }
    return data_p_->get();
  }

  [[nodiscard]] std::future_status check() const {
    if (data_p_->ready()) {
      return std::future_status::ready;
    } else {
      return std::future_status::timeout;
    }
  }

  [[nodiscard]] bool ready() const {
    return data_p_->ready();
  }

 private:
  std::unique_ptr<am_internal::FutureData<T>> data_p_;
};

} // namespace arl
#endif //ARL_FUTURE_HPP
