#ifndef ARL_DIST_WRAPPER_HPP
#define ARL_DIST_WRAPPER_HPP

namespace arl {

  template <typename T>
  class DistWrapper {
  private:
    std::vector<T *> map_ptrs;
  public:
    // initialize the local map
    template<typename ...U>
    DistWrapper(T* ptr) {
      map_ptrs.resize(rank_n());
      map_ptrs[rank_me()] = ptr;
      for (size_t i = 0; i < rank_n(); ++i) {
        broadcast(map_ptrs[i], i);
      }
    }

    ~DistWrapper() {
      // barrier(); // should we explicitly put a barrier in the deconstructor?
    }

    T *local() const {
      return map_ptrs[rank_me()];
    }

    T *operator[](size_t index) const {
      return map_ptrs[index];
    }

//    template<typename RPC_Fn, typename ...Args>
//    Future <std::invoke_result_t<RPC_Fn, rank_t, T*,  Args...>>
//    remote_invoke(RPC_Fn &&rpc_fn, rank_t remote_worker, Args &&...args) {
//      ARL_Assert(remote_worker < rank_n(), "");
//      return std::invoke(std::forward<RPC_Fn>(rpc_fn), remote_worker, map_ptrs[remote_worker],
//                         std::forward<Args>(args)...);
//    }
  };
} // class DistWrapper

#endif //ARL_DIST_WRAPPER_HPP
