//
// Created by jackyan on 3/10/20.
//
#define ARL_DEBUG
#include "arl/arl.hpp"
#include <iostream>
#include <random>

#include "external/typename.hpp"

using std::cout;
using std::endl;
using arl::am_internal::get_pi_fnptr;
using arl::am_internal::resolve_pi_fnptr;

int foo1(char a, int b, bool c) {
//  cout << "Call foo1 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
  return a + b + c;
}

auto foo2 = [](char a, int b, bool c) -> int {
//  cout << "Call foo2 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
  return a + b + c;
};

struct Foo3 {
  int operator()(char a, int b, bool c) const {
//    cout << "Call foo3 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
    return a + b + c;
  }
};

void worker() {
  using namespace arl;

  int num_ops = 1000;
  size_t my_rank = arl::rank_me();
  size_t nworkers = arl::rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);
  std::vector<Future<int>> futures;

  barrier();
  register_amaggrd(foo1, 'a', 123, true);

  for (int i = 0; i < num_ops; i++) {
    int target_rank = distribution(generator);
    auto f = arl::rpc_aggrd(target_rank, foo1, 'a', (int)my_rank, true);
    futures.push_back(std::move(f));
  }

  barrier();

  for (int i = 0; i < num_ops; ++i) {
    int result = futures[i].get();
    if (result != 'a' + my_rank + 1) {
      printf("error!\n");
    }
  }
  print("Pass!\n");
}

int main() {
  using req_invoker_t = int(intptr_t, char*, int, char*, int);
  arl::init(1, 1);

  //---
  cout << "sizeof(AmaggrdReqMeta) is " << sizeof(arl::amaggrd_internal::AmaggrdReqMeta) << endl;
  cout << "sizeof(AmaggrdReqPayload) is " << sizeof(arl::amaggrd_internal::AmaggrdReqPayload<char, int, bool>) << endl;
  cout << "sizeof(AmaggrdAckMeta) is " << sizeof(arl::amaggrd_internal::AmaggrdAckMeta) << endl;
  cout << "sizeof(AmaggrdAckPayload) is " << sizeof(arl::amaggrd_internal::AmaggrdAckPayload<int>) << endl;

  // ---
  using Payload = arl::amaggrd_internal::AmaggrdReqPayload<char, int, bool>;
  using AckPayload = arl::amaggrd_internal::AmaggrdAckPayload<int>;

  intptr_t wrapper_p1 = get_pi_fnptr(arl::amaggrd_internal::AmaggrdTypeWrapper<decltype(foo1), char, int, bool>::invoker);
  auto invoker1 = resolve_pi_fnptr<intptr_t(const std::string&)>(wrapper_p1);
  intptr_t req_invoker_p1 = invoker1("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t >(req_invoker_p1);

  intptr_t fn_p1 = get_pi_fnptr(&foo1);
  arl::Future<int> future1(true), future2(true);
  Payload data1{future1.get_p(), 0, std::make_tuple('a', 233, true)};
  Payload data2{future2.get_p(), 1, std::make_tuple('b', 144, false)};

  arl::amaggrd_internal::AmaggrdReqMeta meta{fn_p1, wrapper_p1};
  gex_AM_Arg_t* meta_p1 = reinterpret_cast<gex_AM_Arg_t*>(&meta);
  char buf[100];
  int buf_len = 0;
  std::memcpy(buf + buf_len, &data1, sizeof(data1));
  buf_len += sizeof(data1);
  std::memcpy(buf + buf_len, &data2, sizeof(data2));
  buf_len += sizeof(data2);
  char out_buf[100];
  int out_len = req_invoker(fn_p1, buf, buf_len, out_buf, 100);
  cout << "out_len is " << out_len << endl;
  cout << "result1 is " << reinterpret_cast<AckPayload*>(out_buf)->data << endl;
  cout << "result2 is " << reinterpret_cast<AckPayload*>(out_buf + sizeof(AckPayload))->data << endl;

  // ---
//  arl::register_amaggrd(foo2, char(), int(), bool());
//  cout << "hidx_generic_amffrd_reqhandler is " << (int)arl::amaggrd_internal::hidx_generic_amaggrd_reqhandler << endl;
//  gex_AM_RequestMedium4(arl::backend::tm, 0, arl::amaggrd_internal::hidx_generic_amaggrd_reqhandler, buf,
//                        buf_len, GEX_EVENT_NOW, 0, meta_p1[0], meta_p1[1], meta_p1[2], meta_p1[3]);
//  cout << "ack counter is " << *arl::amaggrd_internal::amaggrd_ack_counter << endl;
//  cout << "future1.get() is " << future1.get() << endl;
//  cout << "future2.get() is " << future2.get() << endl;
//   ---
//  auto f1 = arl::rpc_aggrd(0, foo2, 'c', 34, true);
//  auto f2 = arl::rpc_aggrd(0, foo2, 'd', 35, true);
//  auto f3 = arl::rpc_aggrd(0, foo2, 'e', 36, true);
//  arl::flush_agg_buffer();
//  arl::wait_am();
//  cout << "ack counter is " << *arl::amaggrd_internal::amaggrd_ack_counter << endl;

  arl::run(worker);
  arl::finalize();
  return 0;
}