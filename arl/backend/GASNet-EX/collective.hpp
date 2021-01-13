//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_BACKEND_COLLECTIVE_HPP
#define ARL_BACKEND_COLLECTIVE_HPP

#include <gasnet_coll.h>

namespace arl::backend {
template <typename T>
inline T broadcast(T& val, rank_t root) {
  T rv;
  gex_Event_t event = gex_Coll_BroadcastNB(internal::tm, root, &rv, &val, sizeof(T), 0);
  progress_until([&](){return !gex_Event_Test(event);});

  return rv;
}
} // namespace arl::backend

#endif //ARL_BACKEND_COLLECTIVE_HPP
