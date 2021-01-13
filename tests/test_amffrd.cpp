//
// Created by jackyan on 3/10/20.
//
#define ARL_DEBUG
#include "arl.hpp"
#include <iostream>
#include <random>

#include "external/typename.hpp"

using std::cout;
using std::endl;
using arl::am_internal::get_pi_fnptr;
using arl::am_internal::resolve_pi_fnptr;

void foo1(char a, int b, bool c) {
//  cout << "Call foo1 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
}

auto foo2 = [](char a, int b, bool c) {
//  cout << "Call foo2 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
};

struct Foo3 {
  void operator()(char a, int b, bool c) const {
//    cout << "Call foo3 on rank " << arl::rank_me() << ": char " << a << ", int " << b << ", bool " << c << endl;
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

  barrier();
  register_amffrd(foo1, 'a', 123, true);

  for (int i = 0; i < num_ops; i++) {
    int target_rank = distribution(generator);
    rpc_ffrd(target_rank, foo1, 'a', (int)my_rank, true);
  }

  barrier();
  print("Pass!\n");
}

int main() {
  using req_invoker_t = int(intptr_t, char*, int);
  arl::init();

/*  //---
  cout << "sizeof(AmffrdReqMeta) is " << sizeof(arl::amffrd_internal::AmffrdReqMeta) << endl;
  cout << "sizeof(AmffrdReqPayload) is " << sizeof(arl::amffrd_internal::AmffrdReqPayload<char, int, bool>) << endl;

  // ---
  using Payload = arl::amffrd_internal::AmffrdReqPayload<char, int, bool>;
  Foo3 foo3;

  intptr_t wrapper_p1 = get_pi_fnptr(arl::amffrd_internal::AmffrdTypeWrapper<decltype(foo1), char, int, bool>::invoker);
  auto invoker1 = resolve_pi_fnptr<intptr_t(const std::string&)>(wrapper_p1);
  intptr_t req_invoker_p1 = invoker1("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t >(req_invoker_p1);

  intptr_t fn_p1 = get_pi_fnptr(&foo1);
  Payload data1{0, std::make_tuple('a', 233, true)};
  Payload data2{1, std::make_tuple('b', 144, false)};

  arl::amffrd_internal::AmffrdReqMeta meta{fn_p1, wrapper_p1};
  gex_AM_Arg_t* meta_p1 = reinterpret_cast<gex_AM_Arg_t*>(&meta);
  char buf[100];
  int buf_len = 0;
  std::memcpy(buf + buf_len, &data1, sizeof(data1));
  buf_len += sizeof(data1);
  std::memcpy(buf + buf_len, &data2, sizeof(data2));
  buf_len += sizeof(data2);
  int ack_n = req_invoker(fn_p1, buf, buf_len);
  cout << "ack_n is " << ack_n << endl;

  // ---
//  arl::register_amffrd(foo2, char(), int(), bool());
//  cout << "sizeof(Payload) is " << sizeof(Payload) << endl;
//  cout << "hidx_generic_amffrd_reqhandler is " << (int)arl::amffrd_internal::hidx_generic_amffrd_reqhandler << endl;
//  gex_AM_RequestMedium4(arl::backend::tm, 0, arl::amffrd_internal::hidx_generic_amffrd_reqhandler, buf,
//                        buf_len, GEX_EVENT_NOW, 0, meta_p1[0], meta_p1[1], meta_p1[2], meta_p1[3]);
//  arl::amffrd_internal::send_amffrd_to_gex(0, meta, buf, buf_len);
  // ---
//  cout << "ack counter is " << *arl::amffrd_internal::amffrd_ack_counter << endl;
//  arl::rpc_ffrd(0, foo1, 'c', 34, true);
//  arl::rpc_ffrd(0, foo1, 'd', 35, true);
//  arl::rpc_ffrd(0, foo1, 'e', 36, true);
//  arl::flush_amffrd_buffer();
//  arl::wait_amffrd();
//  cout << "ack counter is " << *arl::amffrd_internal::amffrd_ack_counter << endl;
*/
  arl::run(worker);
  arl::finalize();
  return 0;
}