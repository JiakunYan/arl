//
// Created by Jiakun Yan on 12/6/19.
//

#ifndef ARL_BASE_COLLECTIVE_HPP
#define ARL_BASE_COLLECTIVE_HPP

namespace arl {
  namespace local {
    template <typename T>
    inline T broadcast(T& val, rank_t root) {
      ARL_Assert(root < arl::nworkers_local(), "");
      static T shared_val;
      if (my_worker_local() == root) {
        shared_val = val;
        local::barrier();
      } else {
        local::barrier();
        val = shared_val;
      }
      local::barrier();
      return val;
    }

  }

  template <typename T>
  inline T broadcast(T& val, rank_t root) {
    ARL_Assert(root < arl::nworkers(), "");
    if (my_worker_local() == root % nworkers_local()) {
      val = backend::broadcast(val, root / nworkers_local());
    }
    local::broadcast(val, root % nworkers_local());
    return val;
  }

  namespace local {
    template <typename T, typename BinaryOp>
    inline T reduce_all(const T& value, const BinaryOp& op) {
      static std::atomic<rank_t> order = -1;
      static T result = T();

      local::barrier();
      if (my_worker_local() == 0) {
        result = value;
        order = 1;
      } else {
        while (order != my_worker_local()) progress();
        result = op(result, value);
        ++order;
      }
      local::barrier();
      return result;
    }
  }

  template <typename T, typename BinaryOp>
  inline T reduce_one(const T& value, const BinaryOp& op, rank_t root) {
    ARL_Assert(root < arl::nworkers(), "");
    T result = local::reduce_all(value, op);
    if (my_worker_local() == root % nworkers_local()) {
      result = backend::reduce_one(result, op, root / nworkers_local());
    }
    if (my_worker() == root) {
      return result;
    } else {
      return T();
    }
  }

  template <typename T, typename BinaryOp>
  inline T reduce_all(const T& value, const BinaryOp& op) {
    T result = local::reduce_all(value, op);
    if (my_worker_local() == 0) {
      result = backend::reduce_all(result, op);
    }
    result = local::broadcast(result, 0);
    return result;
  }
}

#endif //ARL_BASE_COLLECTIVE_HPP
