//
// Created by Jiakun Yan on 10/21/19.
//

#ifndef ARL_ASSERT_HPP
#define ARL_ASSERT_HPP

#include <sys/time.h>
#include <time.h>
#include <iostream>

#ifdef ARL_DEBUG
#   define ARL_Assert(Expr, Msg) \
    __ARL_Assert(#Expr, Expr, __FILE__, __LINE__, Msg)
#else
#   define ARL_Assert(Expr, Msg) ;
#endif
void __ARL_Assert(const char* expr_str, bool expr, const char* file, int line, const char* msg)
{
  if (!expr)
  {
    std::cerr << "Assert failed:\t" << msg << "\n"
              << "Expected:\t" << expr_str << "\n"
              << "Source:\t\t" << file << ", line " << line << "\n";
    abort();
  }
}

#define ARL_Assert_Align(Val, alignof_size) ARL_Assert(alignof(Val) % alignof_size == 0, "alignment check failed!")


#endif //ARL_ASSERT_HPP
