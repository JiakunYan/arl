#ifndef ARL_OP_HPP
#define ARL_OP_HPP

#define Register_Op(name, fun) \
  struct name { \
  template< class T, class U> \
  constexpr auto operator()( T&& lhs, U&& rhs ) const \
  -> decltype(fun(std::forward<T>(lhs), std::forward<U>(rhs))) { \
      return fun(std::forward<T>(lhs), std::forward<U>(rhs)); \
    } \
  }; \

namespace arl {
  Register_Op(op_plus, std::plus<>{})
  Register_Op(op_multiplies, std::multiplies<>{})
  Register_Op(op_bit_and, std::bit_and<>{})
  Register_Op(op_bit_or, std::bit_or<>{})
  Register_Op(op_bit_xor, std::bit_xor<>{})
  Register_Op(op_min, std::min)
  Register_Op(op_max, std::max)
}

#undef Register_Op

#endif //ARL_OP_HPP
