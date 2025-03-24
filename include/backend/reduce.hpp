// This file is simplified based on UPC++(https://bitbucket.org/berkeleylab/upcxx/wiki/Home) reduce.hpp, reduce.cpp
#ifndef ARL_BACKEND_REDUCE_HPP
#define ARL_BACKEND_REDUCE_HPP

namespace arl::backend {

template<int bits, bool is_signed>
struct reduce_ty_id_integral {
  static const datatype_t ty_id;// the MPI Datatype for this type
};

template<int bits>
struct reduce_ty_id_floating {
  static const datatype_t ty_id;// the GEX DT name for this type
};

template<typename T,
         bool is_integral = std::is_integral<T>::value,
         bool is_floating = std::is_floating_point<T>::value>
struct reduce_ty_id {
  static const datatype_t ty_id;// the GEX DT name for this type
};

template<typename T>
struct reduce_ty_id<T,
  /*is_integral=*/true,
  /*is_floating=*/false> : reduce_ty_id_integral</*bits=*/8 * sizeof(T), std::is_signed<T>::value> {
};

template<typename T>
struct reduce_ty_id<T,
  /*is_integral=*/false,
  /*is_floating=*/true> : reduce_ty_id_floating</*bits=*/8 * sizeof(T)> {
};

template<typename OpFn>
struct reduce_op_id {
  static const reduce_op_t op_id;
};

template<typename T, typename BinaryOp>
void reduce_fn(const void *left, const void *right, void *dst, size_t n) {
  const T *l = static_cast<const T *>(left);
  const T *r = static_cast<const T *>(right);
  T *d = static_cast<T *>(dst);
  BinaryOp op;
  for (size_t i = 0; i < n; ++i) {
    d[i] = op(l[i], r[i]);
  }
}

template<typename T, typename BinaryOp>
T reduce_one(const T &value, const BinaryOp &op, rank_t root) {
  T result;
  reduce_one_impl(&value, &result, 1, reduce_ty_id<T>::ty_id,
                  reduce_op_id<BinaryOp>::op_id, reduce_fn<T, BinaryOp>, root);
  return result;
}

template<typename T, typename BinaryOp>
std::vector<T> reduce_one(const std::vector<T> &value, const BinaryOp &op, rank_t root) {
  std::vector<T> result = std::vector<T>(value.size());
  reduce_one_impl(value.data(), result.data(), value.size(), reduce_ty_id<T>::ty_id,
                  reduce_op_id<BinaryOp>::op_id, reduce_fn<T, BinaryOp>, root);
  return result;
}

template<typename T, typename BinaryOp>
T reduce_all(const T &value, const BinaryOp &op) {
  T result;
  reduce_all_impl(&value, &result, 1, reduce_ty_id<T>::ty_id,
                  reduce_op_id<BinaryOp>::op_id, reduce_fn<T, BinaryOp>);
  return result;
}

template<typename T, typename BinaryOp>
std::vector<T> reduce_all(const std::vector<T> &value, const BinaryOp &op) {
  std::vector<T> result = std::vector<T>(value.size());
  reduce_all_impl(value.data(), result.data(), value.size(), reduce_ty_id<T>::ty_id,
                  reduce_op_id<BinaryOp>::op_id, reduce_fn<T, BinaryOp>);
  return result;
}

} // namespace arl::backend

#endif //ARL_BACKEND_REDUCE_HPP