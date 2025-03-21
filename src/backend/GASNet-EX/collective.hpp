//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_BACKEND_COLLECTIVE_HPP
#define ARL_BACKEND_COLLECTIVE_HPP

#include <gasnet_coll.h>

namespace arl::backend::internal {
template<typename T>
inline T broadcast(T &val, rank_t root) {
  T rv;
  gex_Event_t event = gex_Coll_BroadcastNB(internal::tm, root, &rv, &val, sizeof(T), 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });

  return rv;
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_COLLECTIVE_HPP
