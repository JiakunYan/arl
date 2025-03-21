// This file is simplified based on UPC++(https://bitbucket.org/berkeleylab/upcxx/wiki/Home) reduce.hpp, reduce.cpp
#ifndef ARL_BACKEND_REDUCE_HPP
#define ARL_BACKEND_REDUCE_HPP

namespace arl::backend::internal {
template<int bits, bool is_signed>
struct reduce_gex_ty_id_integral {
  static const gex_DT_t ty_id;// the GEX DT name for this type
};

template<int bits>
struct reduce_gex_ty_id_floating {
  static const gex_DT_t ty_id;// the GEX DT name for this type
};

template<typename T,
         bool is_integral = std::is_integral<T>::value,
         bool is_floating = std::is_floating_point<T>::value>
struct reduce_gex_ty_id {
  static const gex_DT_t ty_id;// the GEX DT name for this type
};

template<typename T>
struct reduce_gex_ty_id<T,
                        /*is_integral=*/true,
                        /*is_floating=*/false> : reduce_gex_ty_id_integral</*bits=*/8 * sizeof(T), std::is_signed<T>::value> {
};

template<typename T>
struct reduce_gex_ty_id<T,
                        /*is_integral=*/false,
                        /*is_floating=*/true> : reduce_gex_ty_id_floating</*bits=*/8 * sizeof(T)> {
};

template<typename OpFn>
struct reduce_gex_op_id {
  static const gex_OP_t op_id;// the GEX OP name for this operation (ex: op_id = GEX_OP_ADD when OpFn = opfn_add)
};

template<>
const gex_DT_t reduce_gex_ty_id_integral<32, /*signed=*/true>::ty_id = GEX_DT_I32;
template<>
const gex_DT_t reduce_gex_ty_id_integral<64, /*signed=*/true>::ty_id = GEX_DT_I64;
template<>
const gex_DT_t reduce_gex_ty_id_integral<32, /*signed=*/false>::ty_id = GEX_DT_U32;
template<>
const gex_DT_t reduce_gex_ty_id_integral<64, /*signed=*/false>::ty_id = GEX_DT_U64;
template<>
const gex_DT_t reduce_gex_ty_id_floating<32>::ty_id = GEX_DT_FLT;
template<>
const gex_DT_t reduce_gex_ty_id_floating<64>::ty_id = GEX_DT_DBL;

template<>
const gex_OP_t reduce_gex_op_id<op_plus>::op_id = GEX_OP_ADD;
template<>
const gex_OP_t reduce_gex_op_id<op_multiplies>::op_id = GEX_OP_MULT;
template<>
const gex_OP_t reduce_gex_op_id<op_min>::op_id = GEX_OP_MIN;
template<>
const gex_OP_t reduce_gex_op_id<op_max>::op_id = GEX_OP_MAX;
template<>
const gex_OP_t reduce_gex_op_id<op_bit_and>::op_id = GEX_OP_AND;
template<>
const gex_OP_t reduce_gex_op_id<op_bit_or>::op_id = GEX_OP_OR;
template<>
const gex_OP_t reduce_gex_op_id<op_bit_xor>::op_id = GEX_OP_XOR;

template<typename T, typename BinaryOp>
inline T reduce_one(const T &value, const BinaryOp &op, rank_t root) {
  T result = T();
  gex_Event_t event = gex_Coll_ReduceToOneNB(
          internal::tm, root, &result, &value,
          reduce_gex_ty_id<T>::ty_id, sizeof(T), 1,
          reduce_gex_op_id<BinaryOp>::op_id,
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
  return result;
}

template<typename T, typename BinaryOp>
inline std::vector<T> reduce_one(const std::vector<T> &value, const BinaryOp &op, rank_t root) {
  std::vector<T> result = std::vector<T>(value.size());
  gex_Event_t event = gex_Coll_ReduceToOneNB(
          internal::tm, root, result.data(), value.data(),
          reduce_gex_ty_id<T>::ty_id, sizeof(T), value.size(),
          reduce_gex_op_id<BinaryOp>::op_id,
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
  if (backend::rank_me() == root) {
    return result;
  } else {
    return std::vector<T>();
  }
}

template<typename T, typename BinaryOp>
inline T reduce_all(const T &value, const BinaryOp &op) {
  T result = T();
  gex_Event_t event = gex_Coll_ReduceToAllNB(
          internal::tm, &result, &value,
          reduce_gex_ty_id<T>::ty_id, sizeof(T), 1,
          reduce_gex_op_id<BinaryOp>::op_id,
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
  return result;
}

template<typename T, typename BinaryOp>
inline std::vector<T> reduce_all(const std::vector<T> &value, const BinaryOp &op) {
  std::vector<T> result = std::vector<T>(value.size());
  gex_Event_t event = gex_Coll_ReduceToAllNB(
          internal::tm, result.data(), value.data(),
          reduce_gex_ty_id<T>::ty_id, sizeof(T), value.size(),
          reduce_gex_op_id<BinaryOp>::op_id,
          NULL, NULL, 0);
  progress_external_until([&]() {
    int done = !gex_Event_Test(event);
    if (!done)
      CHECK_GEX(gasnet_AMPoll());
    return done;
  });
  return result;
}
}// namespace arl::backend::internal

#endif//ARL_BACKEND_REDUCE_HPP
