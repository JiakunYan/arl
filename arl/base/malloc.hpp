//
// Created by jackyan on 2/26/20.
//

#ifndef ARL_MALLOC_HPP
#define ARL_MALLOC_HPP

namespace arl {
  namespace local {
    template <typename T>
    inline T* alloc() {
      T* ptr;
      if (local::rank_me() == 0) {
        ptr = new T();
      }
      local::broadcast(ptr, 0);
      return ptr;
    }

    template <typename T>
    inline void dealloc(T* ptr) {
      local::barrier();
      if (local::rank_me() == 0) {
        delete ptr;
      }
    }
  }
}

#endif //ARL_MALLOC_HPP
