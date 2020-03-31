//
// Created by jackyan on 3/10/20.
//
#define ARL_DEBUG
#include "arl/arl.hpp"
#include <iostream>

#include "external/typename.hpp"

using std::cout;
using std::endl;
using arl::am_internal::get_pi_fnptr;
using arl::am_internal::resolve_pi_fnptr;
using arl::amff_internal::AmffReqMeta;

void foo1(char a, int b, bool c) {
  cout << "Call foo1: " << "char " << a << ", int " << b << ", bool " << c << endl;
}

auto foo2 = [](char a, int b, bool c) {
  cout << "Call foo2: " << "char " << a << ", int " << b << ", bool " << c << endl;
};

struct Foo3 {
  void operator()(char a, int b, bool c) const {
    cout << "Call foo3: " << "char " << a << ", int " << b << ", bool " << c << endl;
  }
};

int main() {
  arl::init();

  //---
  cout << "sizeof(AmffReqMeta) is " << sizeof(arl::amff_internal::AmffReqMeta) << endl;

  // ---
  Foo3 foo3;

  intptr_t wrapper_p = get_pi_fnptr(arl::amff_internal::AmffTypeWrapper<decltype(foo1), char, int, bool>::invoker);
  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<void(intptr_t, int, char*, int)>(req_invoker_p);

  intptr_t fn_p = get_pi_fnptr(&foo1);
  std::tuple<char, int, bool> data{'a', 233, true};
  req_invoker(fn_p, 1, reinterpret_cast<char*>(&data), sizeof(data));

  // ---
  using Payload = std::tuple<char, int ,bool>;
  cout << "sizeof(Payload) is " << sizeof(Payload) << endl;
  intptr_t foo1_p = get_pi_fnptr(foo1);
  intptr_t wrapper2_p = get_pi_fnptr(arl::amff_internal::AmffTypeWrapper<decltype(foo2), char, int, bool>::invoker);
  intptr_t foo2_p = get_pi_fnptr(&foo2);
  char buf[100];
  char* ptr = buf;
  int offset = 0;
  *reinterpret_cast<AmffReqMeta*>(ptr) = AmffReqMeta{foo1_p, wrapper_p, 0};
  offset += sizeof(AmffReqMeta);
  *reinterpret_cast<Payload*>(ptr + offset) = data;
  offset += sizeof(Payload);
  *reinterpret_cast<AmffReqMeta*>(ptr + offset) = AmffReqMeta{foo2_p, wrapper2_p, 0};
  offset += sizeof(AmffReqMeta);
  *reinterpret_cast<Payload*>(ptr + offset) = Payload{'b', 134, false};
  if (arl::rank_me() == 0) {
    gex_AM_RequestMedium0(arl::backend::tm, 0, arl::amff_internal::hidx_generic_am_ff_reqhandler, buf,
                          offset + sizeof(Payload), GEX_EVENT_NOW, 0);
  }

  // ---
  arl::rpc_ff(0, foo1, 'c', 34, true);
  arl::flush_agg_buffer();
  arl::flush_am();
  cout << "ack counter is " << *arl::am_internal::am_ack_counter << endl;

  arl::finalize();
  return 0;
}