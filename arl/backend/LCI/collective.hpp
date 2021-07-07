//
// Created by Jiakun Yan on 1/1/20.
//

#ifndef ARL_BACKEND_COLLECTIVE_HPP
#define ARL_BACKEND_COLLECTIVE_HPP

namespace arl::backend {
template <typename T>
inline T broadcast(T& val, rank_t root) {
  T rv = val;
  MPI_Request request;
  MPI_Ibcast(&rv, sizeof(T), MPI_BYTE, root, MPI_COMM_WORLD, &request);
  progress_external_until([&](){
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });

  return rv;
}
} // namespace arl::backend

#endif //ARL_BACKEND_COLLECTIVE_HPP
