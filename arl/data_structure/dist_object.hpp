#ifndef ARL_DIST_OBJECT_HPP
#define ARL_DIST_OBJECT_HPP

namespace arl {

  template <typename T>
  class DistObject {
  private:
    struct LocalObject {
      T val;
      std::mutex lock;

      template<typename ...U>
      LocalObject(U &&...arg): val(std::forward<U>(arg)...) {}
    };

    std::vector<LocalObject *> map_ptrs;
  public:
    // initialize the local map
    template<typename ...U>
    DistObject(U &&...arg) {
      map_ptrs.resize(proc::rank_n());
      if (local::rank_me() == 0) {
        map_ptrs[proc::rank_me()] = new LocalObject(std::forward<U>(arg)...);
      }
      for (size_t i = 0; i < proc::rank_n(); ++i) {
        broadcast(map_ptrs[i], i * local::rank_n());
      }
    }

    ~DistObject() {
      arl::barrier();
      if (local::rank_me() == 0) {
        delete map_ptrs[proc::rank_me()];
      }
    }

    T *operator->() const { return const_cast<T *>(&(map_ptrs[proc::rank_me()]->val)); }

    T &operator*() const { return const_cast<T &>(map_ptrs[proc::rank_me()]->val); }

    template<typename Fn, typename ...Args>
    Future <std::invoke_result_t<Fn, Args...>>
    remote_invoke(rank_t remote_worker, Fn &&fn, Args &&...args) {
      ARL_Assert(remote_worker < rank_n(), "");
      ARL_Error("Has not been implemented!");
      return std::invoke(std::forward(fn), std::forward(args...));
    }
  }
}

#endif //ARL_DIST_OBJECT_HPP
