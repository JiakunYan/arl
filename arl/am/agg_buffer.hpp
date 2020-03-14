//
// Created by jackyan on 3/11/20.
//

#ifndef ARL_AGG_BUFFER_HPP
#define ARL_AGG_BUFFER_HPP

namespace arl{
extern void progress();
namespace am_internal {

class AggBufferSimple {
 public:
  AggBufferSimple(): cap_(0), tail_(0), ptr_(nullptr) {
  }

  void init(int cap) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    tail_ = 0;
    delete [] ptr_;
    ptr_ = new char[cap];
  }

  ~AggBufferSimple() {
    delete [] ptr_;
  }

  template <typename T, typename U>
  pair<char*, int> push(const T& val1, const U& val2) {
    while (!lock.try_lock()) {
      progress();
    }
    pair<char*, int> result(nullptr, 0);
    if (tail_ + sizeof(val1) + sizeof(val2) > cap_) {
      result = make_pair(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &val1, sizeof(T));
    tail_ += sizeof(val1);
    std::memcpy(ptr_ + tail_, &val2, sizeof(U));
    tail_ += sizeof(val2);
    lock.unlock();
    return result;
  }

  template <typename T>
  pair<char*, int> push(const T& value) {
    while (!lock.try_lock()) {
      progress();
    }
    pair<char*, int> result(nullptr, 0);
    if (tail_ + sizeof(value) > cap_) {
      result = make_pair(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    std::memcpy(ptr_ + tail_, &value, sizeof(T));
    tail_ += sizeof(value);
    lock.unlock();
    return result;
  }

  vector<pair<char*, int>> flush() {
    while (!lock.try_lock()) {
      progress();
    }
    vector<pair<char*, int>> result;
    if (tail_ > 0) {
      result.emplace_back(ptr_, tail_);
      ptr_ = new char [cap_];
      tail_ = 0;
    }
    lock.unlock();
    return result;
  }

 private:
  char* ptr_;
  int cap_;
  int tail_;
  mutex lock;
};

class AggBufferLocal {
 public:
  AggBufferLocal(): cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete [] thread_tail_;
    thread_tail_ = new int [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i] = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i];
      }
      delete [] thread_ptr_;
    }
    thread_ptr_ = new char*[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i] = new char[cap];
    }
  }

  ~AggBufferLocal() {
    delete [] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i];
      }
      delete [] thread_ptr_;
    }
  }

  template <typename T, typename U>
  pair<char*, int> push(const T& val1, const U& val2) {
    int my_rank = local::rank_me();
    pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank] + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = make_pair(thread_ptr_[my_rank], thread_tail_[my_rank]);
      thread_ptr_[my_rank] = new char [cap_];
      thread_tail_[my_rank] = 0;
    }
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &val1, sizeof(val1));
    thread_tail_[my_rank] += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &val2, sizeof(val2));
    thread_tail_[my_rank] += sizeof(val2);
    return result;
  }

  template <typename T>
  pair<char*, int> push(const T& value) {
    int my_rank = local::rank_me();
    pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank] + sizeof(value) > cap_) {
      // push my
      result = make_pair(thread_ptr_[my_rank], thread_tail_[my_rank]);
      thread_ptr_[my_rank] = new char [cap_];
      thread_tail_[my_rank] = 0;
    }
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &value, sizeof(T));
    thread_tail_[my_rank] += sizeof(value);
    return result;
  }

  vector<pair<char*, int>> flush() {
//    vector<pair<int, int>> data;
//    for (int i = 0; i < thread_num_; ++i) {
//      data.emplace_back(i, thread_tail_[i]);
//    }
//    std::sort(data.begin(), data.end(),
//              [](pair<int, int> a, pair<int, int> b) {
//                return (get<1>(a) < get<1>(b));
//              });
    int my_rank = local::rank_me();
    vector<pair<char*, int>> results;
    if (thread_tail_[my_rank] > 0) {
      results.emplace_back(thread_ptr_[my_rank], thread_tail_[my_rank]);
      thread_ptr_[my_rank] = new char [cap_];
      thread_tail_[my_rank] = 0;
    }
    return results;
  }

 private:
  char** thread_ptr_;
  int* thread_tail_;
  int cap_;
  int thread_num_;
};

class AggBufferAdvanced {
 public:
  AggBufferAdvanced(): cap_(0), thread_num_(0), thread_tail_(nullptr), thread_ptr_(nullptr), lock_ptr_(nullptr) {
  }

  void init(int cap, int thread_num = local::rank_n()) {
    ARL_Assert(cap > 0);
    cap_ = cap;
    thread_num_ = thread_num;

    delete [] lock_ptr_;
    lock_ptr_ = new mutex [thread_num_];

    delete [] thread_tail_;
    thread_tail_ = new int [thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_tail_[i] = 0;
    }

    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i];
      }
      delete [] thread_ptr_;
    }
    thread_ptr_ = new char*[thread_num_];
    for (int i = 0; i < thread_num_; ++i) {
      thread_ptr_[i] = new char[cap];
    }
  }

  ~AggBufferAdvanced() {
    delete [] lock_ptr_;
    delete [] thread_tail_;
    if (thread_ptr_ != nullptr) {
      for (int i = 0; i < thread_num_; ++i) {
        delete [] thread_ptr_[i];
      }
      delete [] thread_ptr_;
    }
  }

  template <typename T, typename U>
  pair<char*, int> push(const T& val1, const U& val2) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].try_lock()) {
      progress();
    }
    pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank] + sizeof(val1) + sizeof(val2) > cap_) {
      // push my
      result = make_pair(thread_ptr_[my_rank], thread_tail_[my_rank]);
      thread_ptr_[my_rank] = new char [cap_];
      thread_tail_[my_rank] = 0;
    }
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &val1, sizeof(val1));
    thread_tail_[my_rank] += sizeof(val1);
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &val2, sizeof(val2));
    thread_tail_[my_rank] += sizeof(val2);
    lock_ptr_[my_rank].unlock();
    return result;
  }

  template <typename T>
  pair<char*, int> push(const T& value) {
    int my_rank = local::rank_me();
    while (!lock_ptr_[my_rank].try_lock()) {
      progress();
    }
    pair<char*, int> result(nullptr, 0);
    if (thread_tail_[my_rank] + sizeof(value) > cap_) {
      // push my
      result = make_pair(thread_ptr_[my_rank], thread_tail_[my_rank]);
      thread_ptr_[my_rank] = new char [cap_];
      thread_tail_[my_rank] = 0;
    }
    std::memcpy(thread_ptr_[my_rank] + thread_tail_[my_rank], &value, sizeof(T));
    thread_tail_[my_rank] += sizeof(value);
    lock_ptr_[my_rank].unlock();
    return result;
  }

  vector<pair<char*, int>> flush() {
//    vector<pair<int, int>> data;
//    for (int i = 0; i < thread_num_; ++i) {
//      data.emplace_back(i, thread_tail_[i]);
//    }
//    std::sort(data.begin(), data.end(),
//              [](pair<int, int> a, pair<int, int> b) {
//                return (get<1>(a) < get<1>(b));
//              });
    int my_rank = local::rank_me();
    vector<pair<char*, int>> results;
    for (int ii = 0; ii < thread_num_; ++ii) {
      int i = (ii + my_rank) % thread_num_;
      while (!lock_ptr_[i].try_lock()) {
        progress();
      }
      if (thread_tail_[i] > 0) {
        results.emplace_back(thread_ptr_[i], thread_tail_[i]);
        thread_ptr_[i] = new char [cap_];
        thread_tail_[i] = 0;
      }
      lock_ptr_[i].unlock();
    }
    return results;
  }

 private:
  char** thread_ptr_;
  int* thread_tail_;
  int cap_;
  int thread_num_;
  mutex* lock_ptr_;
};

//using AggBuffer = AggBufferSimple;
//using AggBuffer = AggBufferLocal;
using AggBuffer = AggBufferAdvanced;

} // namespace am_internal
} // namespace arl

#endif //ARL_AGG_BUFFER_HPP
