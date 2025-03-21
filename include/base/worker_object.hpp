#ifndef ARL_WORKER_OBJECT_HPP
#define ARL_WORKER_OBJECT_HPP

namespace arl {

template<typename T>
struct WorkerObject {
#ifdef ARL_DEBUG
  size_t len = -1;
#endif
  struct align_T {
    alignas(alignof_cacheline) T _val;
    align_T() {
      ARL_Assert_Align(_val, alignof_cacheline);
    }
    explicit align_T(T &&val) : _val(std::forward(val)) {
      ARL_Assert_Align(_val, alignof_cacheline);
    }
  };

  std::vector<align_T> objects;

  void init() {
#ifdef ARL_DEBUG
    len = local::rank_n();
#endif
    objects = std::vector<align_T>(local::rank_n());
  }

  // T must have copy constructor
  void init(T &&val) {
#ifdef ARL_DEBUG
    len = local::rank_n();
#endif
    objects = std::vector<align_T>(local::rank_n(), val);
  }

  T &get() {
#ifdef ARL_DEBUG
    ARL_Assert(len != 1, "Use before calling init!");
    ARL_Assert(local::rank_me() < len, "Index out of scope!");
#endif
    return objects[local::rank_me()]._val;
  }
};

}// namespace arl

#endif//ARL_WORKER_OBJECT_HPP
