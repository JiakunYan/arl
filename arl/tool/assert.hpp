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

#   define ARL_Error(Msg) \
    __ARL_Error(__FILE__, __LINE__, Msg)
void __ARL_Error(const char* file, int line, const char* msg)
{
  std::cerr << "ERROR:\t" << msg << "\n"
            << "Source:\t\t" << file << ", line " << line << std::endl;
  abort();
}

#   define ARL_Warn(Msg) \
    __ARL_Warn(__FILE__, __LINE__, Msg)
void __ARL_Warn(const char* file, int line, const char* msg)
{
  std::cerr << "WARNING:\t" << msg << "\n"
            << "Source:\t\t" << file << ", line " << line << std::endl;
}

#define ARL_Assert_Align(Val, alignof_size) ARL_Assert(alignof(Val) % alignof_size == 0, "alignment check failed!")

#endif //ARL_ASSERT_HPP
