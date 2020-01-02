//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_BACKEND_COLLECTIVE_HPP
#define ARL_BACKEND_COLLECTIVE_HPP

#include <gasnet_coll.h>

namespace arl::backend {
  template <typename T>
  inline T broadcast(T& val, uint64_t root) {
    T rv;
    gex_Event_t event = gex_Coll_BroadcastNB(tm, root, &rv, &val, sizeof(T), 0);
    gex_Event_Wait(event);

    return rv;
  }
}

#endif //ARL_BACKEND_COLLECTIVE_HPP
