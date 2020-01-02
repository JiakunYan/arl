//
// Created by Jiakun Yan on 12/6/19.
//

#ifndef ARL_BASE_COLLECTIVE_HPP
#define ARL_BASE_COLLECTIVE_HPP

namespace arl {
  template <typename T>
  inline T broadcast_node(T& val, uint64_t root) {
    ARL_Assert(root < arl::nworkers_local(), "");
    static T shared_val;
    if (my_worker_local() == root) {
      shared_val = val;
      threadBarrier.wait();
    } else {
      threadBarrier.wait();
      val = shared_val;
    }
    threadBarrier.wait();
    return val;
  }

  template <typename T>
  inline T broadcast(T& val, uint64_t root) {
    ARL_Assert(root < arl::nworkers(), "");
    if (my_worker_local() == root % nworkers_local()) {
      val = backend::broadcast(val, root / nworkers_local());
    }
    broadcast_node(val, root % nworkers_local());
    return val;
  }
}

#endif //ARL_BASE_COLLECTIVE_HPP
