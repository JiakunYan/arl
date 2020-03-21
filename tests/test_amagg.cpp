//
// Created by jackyan on 3/20/20.
//
#define ARL_DEBUG
#include "arl/arl.hpp"
#include <iostream>

#include "external/typename.hpp"

using std::cout;
using std::endl;
using arl::am_internal::get_pi_fnptr;
using arl::am_internal::resolve_pi_fnptr;
using arl::amagg_internal::AmaggReqMeta;

using namespace std;

int foo1(char a, int b, bool c) {
  cout << "Call foo1: " << "char " << a << ", int " << b << ", bool " << c << endl;
  return a + b + c;
}

auto foo2 = [](char a, int b, bool c) -> int {
  cout << "Call foo2: " << "char " << a << ", int " << b << ", bool " << c << endl;
  return a + b + c;
};

struct Foo3 {
  int operator()(char a, int b, bool c) const {
    cout << "Call foo3: " << "char " << a << ", int " << b << ", bool " << c << endl;
    return a + b + c;
  }
};

int main() {
  arl::init();

  //---
  cout << "sizeof(AmaggReqMeta) is " << sizeof(arl::amagg_internal::AmaggReqMeta) << endl;
  cout << "sizeof(AmaggAckMeta) is " << sizeof(arl::amagg_internal::AmaggAckMeta) << endl;

  // ---
  intptr_t wrapper_p = get_pi_fnptr(arl::amagg_internal::AmaggTypeWrapper<decltype(foo1), char, int, bool>::invoker);
  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<int(intptr_t, int, char*, int, char*, int)>(req_invoker_p);

  char result[4];
  intptr_t fn_p = get_pi_fnptr(&foo1);
  tuple<char, int, bool> data{'a', 233, true};
  req_invoker(fn_p, 1, reinterpret_cast<char*>(&data), sizeof(data), result, 4);
  cout << "Result is " << *reinterpret_cast<int*>(result) << endl;

  // ---
  cout << "\n-----------------------\n";
  using Payload = tuple<char, int ,bool>;
  cout << "sizeof(Payload) is " << sizeof(Payload) << endl;
  intptr_t foo1_p = get_pi_fnptr(foo1);
  intptr_t wrapper2_p = get_pi_fnptr(arl::amagg_internal::AmaggTypeWrapper<decltype(foo2), char, int, bool>::invoker);
  intptr_t foo2_p = get_pi_fnptr(&foo2);
  char buf[100];
  char* ptr = buf;
  int offset = 0;
  arl::Future<int> future1;
  arl::Future<int> future2;
  *reinterpret_cast<AmaggReqMeta*>(ptr) = AmaggReqMeta{foo1_p, wrapper_p, future1.get_p(), 0};
  offset += sizeof(AmaggReqMeta);
  *reinterpret_cast<Payload*>(ptr + offset) = data;
  offset += sizeof(Payload);
  *reinterpret_cast<AmaggReqMeta*>(ptr + offset) = AmaggReqMeta{foo2_p, wrapper2_p, future2.get_p(), 0};
  offset += sizeof(AmaggReqMeta);
  *reinterpret_cast<Payload*>(ptr + offset) = Payload{'b', 134, false};
  gex_AM_RequestMedium0(arl::backend::tm, 0, arl::amagg_internal::hidx_generic_amagg_reqhandler, buf,
                        offset + sizeof(Payload), GEX_EVENT_NOW, 0);
  cout << "future1.get() is " << future1.get() << endl;
  cout << "future2.get() is " << future2.get() << endl;
  cout << "ack counter is " << *arl::am_internal::am_ack_counter << endl;

  // ---
  auto future3 = arl::rpc_agg(0, foo1, 'c', 34, true);
  auto future4 = arl::rpc_agg(0, foo2, 'd', 344, true);
  arl::flush_agg_buffer();
  arl::proc::barrier();
  cout << "future3.get() is " << future3.get() << endl;
  cout << "future4.get() is " << future4.get() << endl;

  arl::finalize();
  return 0;
}