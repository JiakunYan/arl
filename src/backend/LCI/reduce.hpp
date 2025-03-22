// This file is simplified based on UPC++(https://bitbucket.org/berkeleylab/upcxx/wiki/Home) reduce.hpp, reduce.cpp
#ifndef ARL_BACKEND_REDUCE_HPP
#define ARL_BACKEND_REDUCE_HPP

namespace arl::backend::internal {
template<int bits, bool is_signed>
struct reduce_mpi_ty_id_integral {
  static const MPI_Datatype ty_id;// the MPI Datatype for this type
};

template<int bits>
struct reduce_mpi_ty_id_floating {
  static const MPI_Datatype ty_id;// the GEX DT name for this type
};

template<typename T,
         bool is_integral = std::is_integral<T>::value,
         bool is_floating = std::is_floating_point<T>::value>
struct reduce_mpi_ty_id {
  static const MPI_Datatype ty_id;// the GEX DT name for this type
};

template<typename T>
struct reduce_mpi_ty_id<T,
                        /*is_integral=*/true,
                        /*is_floating=*/false> : reduce_mpi_ty_id_integral</*bits=*/8 * sizeof(T), std::is_signed<T>::value> {
};

template<typename T>
struct reduce_mpi_ty_id<T,
                        /*is_integral=*/false,
                        /*is_floating=*/true> : reduce_mpi_ty_id_floating</*bits=*/8 * sizeof(T)> {
};

template<typename OpFn>
struct reduce_mpi_op_id {
  static const MPI_Op op_id;// the MPI OP name for this operation (ex: op_id = GEX_OP_ADD when OpFn = opfn_add)
};

template<typename T, typename BinaryOp>
T reduce_one(const T &value, const BinaryOp &op, rank_t root) {
  T result = T();
  MPI_Request request;
  MPI_Ireduce(&value, &result, 1, reduce_mpi_ty_id<T>::ty_id,
              reduce_mpi_op_id<BinaryOp>::op_id, root, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
  return result;
}

template<typename T, typename BinaryOp>
std::vector<T> reduce_one(const std::vector<T> &value, const BinaryOp &op, rank_t root) {
  std::vector<T> result = std::vector<T>(value.size());
  MPI_Request request;
  MPI_Ireduce(value.data(), result.data(), value.size(), reduce_mpi_ty_id<T>::ty_id,
              reduce_mpi_op_id<BinaryOp>::op_id, root, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
  if (backend::rank_me() == root) {
    return result;
  } else {
    return std::vector<T>();
  }
}

template<typename T, typename BinaryOp>
T reduce_all(const T &value, const BinaryOp &op) {
  T result = T();
  MPI_Request request;
  MPI_Iallreduce(&value, &result, 1, reduce_mpi_ty_id<T>::ty_id,
                 reduce_mpi_op_id<BinaryOp>::op_id, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
  return result;
}

template<typename T, typename BinaryOp>
std::vector<T> reduce_all(const std::vector<T> &value, const BinaryOp &op) {
  std::vector<T> result = std::vector<T>(value.size());
  MPI_Request request;
  MPI_Iallreduce(value.data(), result.data(), value.size(), reduce_mpi_ty_id<T>::ty_id,
                 reduce_mpi_op_id<BinaryOp>::op_id, MPI_COMM_WORLD, &request);
  progress_external_until([&]() {
    int flag;
    MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    return flag;
  });
  return result;
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_REDUCE_HPP
