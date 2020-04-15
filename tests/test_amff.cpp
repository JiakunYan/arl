//
// Created by jackyan on 3/10/20.
//
#define ARL_DEBUG
#include "arl/arl.hpp"
#include <iostream>

#include "external/typename.hpp"
#include <random>

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

void worker() {
  using namespace arl;

  int num_ops = 10;
  size_t my_rank = arl::rank_me();
  size_t nworkers = arl::rank_n();
  std::default_random_engine generator(my_rank);
  std::uniform_int_distribution<int> distribution(0, nworkers-1);
  distribution(generator);

  barrier();
  tick_t start = ticks_now();

  for (int i = 0; i < num_ops; i++) {
    int target_rank = distribution(generator);
    rpc_ff(target_rank, foo1, 'a', (int)my_rank, true);
  }

  local::barrier();
  int expected_recv_num = arl::amff_internal::get_expected_recv_num();
  if (rank_me() == 0) {
    std::cout << "recv counter is " << arl::amff_internal::amff_recv_counter->val << endl;
    std::cout << "expected num is " << expected_recv_num << endl;
  }
  barrier();
  tick_t end_wait = ticks_now();

  double duration_total = ticks_to_us(end_wait - start);
  print("rpc_ff overhead is %.2lf us (total %.2lf s)\n", duration_total / num_ops, duration_total / 1e6);
  print("Total single-direction node bandwidth: %lu M/s\n", (unsigned long) (num_ops * local::rank_n() * 2 / duration_total));
}

int main() {
  arl::init();

  //---
//  cout << "sizeof(AmffReqMeta) is " << sizeof(arl::amff_internal::AmffReqMeta) << endl;

  // ---
//  Foo3 foo3;

//  intptr_t wrapper_p = get_pi_fnptr(arl::amff_internal::AmffTypeWrapper<decltype(foo1), char, int, bool>::invoker);
//  auto invoker = resolve_pi_fnptr<intptr_t(const std::string&)>(wrapper_p);
//  intptr_t req_invoker_p = invoker("req_invoker");
//  auto req_invoker = resolve_pi_fnptr<void(intptr_t, int, char*, int)>(req_invoker_p);
//
//  intptr_t fn_p = get_pi_fnptr(&foo1);
//  std::tuple<char, int, bool> data{'a', 233, true};
//  req_invoker(fn_p, 1, reinterpret_cast<char*>(&data), sizeof(data));
//
//   ---
//  using Payload = std::tuple<char, int ,bool>;
//  cout << "sizeof(Payload) is " << sizeof(Payload) << endl;
//  intptr_t foo1_p = get_pi_fnptr(foo1);
//  intptr_t wrapper2_p = get_pi_fnptr(arl::amff_internal::AmffTypeWrapper<decltype(foo2), char, int, bool>::invoker);
//  intptr_t foo2_p = get_pi_fnptr(&foo2);
//  char buf[100];
//  char* ptr = buf;
//  int offset = 0;
//  *reinterpret_cast<AmffReqMeta*>(ptr) = AmffReqMeta{foo1_p, wrapper_p, 0};
//  offset += sizeof(AmffReqMeta);
//  *reinterpret_cast<Payload*>(ptr + offset) = data;
//  offset += sizeof(Payload);
//  *reinterpret_cast<AmffReqMeta*>(ptr + offset) = AmffReqMeta{foo2_p, wrapper2_p, 0};
//  offset += sizeof(AmffReqMeta);
//  *reinterpret_cast<Payload*>(ptr + offset) = Payload{'b', 134, false};
//  if (arl::rank_me() == 0) {
//    gex_AM_RequestMedium0(arl::backend::tm, 0, arl::amff_internal::hidx_generic_am_ff_reqhandler, buf,
//                          offset + sizeof(Payload), GEX_EVENT_NOW, 0);
//  }

  // ---
//  arl::rpc_ff(0, foo1, 'c', 34, true);
//  arl::flush_agg_buffer();
//  cout << "recv counter is " << arl::amff_internal::amff_recv_counter->val << endl;
//  arl::wait_am();
//  cout << "recv counter is " << arl::amff_internal::amff_recv_counter->val << endl;

  arl::run(worker);

  arl::finalize();
  return 0;
}