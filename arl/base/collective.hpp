//
// Created by Jiakun Yan on 12/6/19.
//

#ifndef ARL_BASE_COLLECTIVE_HPP
#define ARL_BASE_COLLECTIVE_HPP

namespace arl {

alignas(alignof_cacheline) ThreadBarrier threadBarrier;

inline void pure_barrier() {
  threadBarrier.wait();
  if (local::rank_me() == 0) {
    backend::barrier();
  }
  threadBarrier.wait();
}

inline void barrier() {
  threadBarrier.wait();
  flush_am();
  if (local::rank_me() == 0) {
    backend::barrier();
  }
  threadBarrier.wait();
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

namespace local {
template <typename T>
inline T broadcast(T& val, rank_t root) {
  ARL_Assert(root < arl::local::rank_n(), "");
  static T shared_val;
  if (local::rank_me() == root) {
    shared_val = val;
    local::barrier();
  } else {
    local::barrier();
    val = shared_val;
  }
  local::barrier();
  return val;
}
} // namespace local

namespace proc {
template <typename T>
inline T broadcast(T& val, rank_t root) {
  return backend::broadcast(val, root);
}
} // namespace proc

template <typename T>
inline T broadcast(T& val, rank_t root) {
  ARL_Assert(root < arl::rank_n(), "");
  if (local::rank_me() == root % local::rank_n()) {
    val = backend::broadcast(val, root / local::rank_n());
  }
  local::broadcast(val, root % local::rank_n());
  return val;
}

namespace local {
template <typename T, typename BinaryOp>
inline T reduce_all(const T& value, const BinaryOp& op) {
  static std::atomic<rank_t> order = -1;
  static T result = T();

  local::barrier();
  if (local::rank_me() == 0) {
    result = value;
    order = 1;
  } else {
    progress_until([&](){return order == local::rank_me();});
    result = op(result, value);
    ++order;
  }
  local::barrier();
  return result;
}

template <typename T, typename BinaryOp>
inline std::vector<T> reduce_all(const std::vector<T>& value, const BinaryOp& op) {
  static std::atomic<rank_t> order = -1;
  static std::vector<T> result;

  local::barrier();
  if (local::rank_me() == 0) {
    result = value;
    order = 1;
  } else {
    progress_until([&](){return order == local::rank_me();});
    ARL_Assert(result.size() == value.size());
    for (int i = 0; i < value.size(); ++i) {
      result[i] = op(result[i], value[i]);
    }
    ++order;
  }
  local::barrier();
  return result;
}
} // namespace local

namespace proc {
template <typename T, typename BinaryOp>
inline T reduce_one(const T& value, const BinaryOp& op, rank_t root) {
  return backend::reduce_one(value, op, root);
}

template <typename T, typename BinaryOp>
inline std::vector<T> reduce_one(const std::vector<T>& value, const BinaryOp& op, rank_t root) {
  return backend::reduce_one(value, op, root);
}

template <typename T, typename BinaryOp>
inline T reduce_all(const T& value, const BinaryOp& op) {
  return backend::reduce_all(value, op);
}

template <typename T, typename BinaryOp>
inline std::vector<T> reduce_all(const std::vector<T>& value, const BinaryOp& op) {
  return backend::reduce_all(value, op);
}
} // namespace proc

template <typename T, typename BinaryOp>
inline T reduce_one(const T& value, const BinaryOp& op, rank_t root) {
  ARL_Assert(root < arl::rank_n(), "");
  T result = local::reduce_all(value, op);
  if (local::rank_me() == root % local::rank_n()) {
    result = backend::reduce_one(result, op, root / local::rank_n());
  }
  if (rank_me() == root) {
    return result;
  } else {
    return T();
  }
}

template <typename T, typename BinaryOp>
inline std::vector<T> reduce_one(const std::vector<T>& value, const BinaryOp& op, rank_t root) {
  ARL_Assert(root < arl::rank_n(), "");
  std::vector<T> result = local::reduce_all(value, op);
  if (local::rank_me() == root % local::rank_n()) {
    result = backend::reduce_one(result, op, root / local::rank_n());
  }
  if (rank_me() == root) {
    return result;
  } else {
    return std::vector<T>();
  }
}

template <typename T, typename BinaryOp>
inline T reduce_all(const T& value, const BinaryOp& op) {
  T result = local::reduce_all(value, op);
  if (local::rank_me() == 0) {
    result = backend::reduce_all(result, op);
  }
  result = local::broadcast(result, 0);
  return result;
}

template <typename T, typename BinaryOp>
inline std::vector<T> reduce_all(const std::vector<T>& value, const BinaryOp& op) {
  std::vector<T> result = local::reduce_all(value, op);
  if (local::rank_me() == 0) {
    result = backend::reduce_all(result, op);
  }
  result = local::broadcast(result, 0);
  return result;
}
} // namespace arl

#endif //ARL_BASE_COLLECTIVE_HPP
