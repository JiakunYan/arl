//
// Created by jackyan on 3/20/20.
//
#define ARL_DEBUG
#include "arl.hpp"
#include <iostream>
#include <random>

#include "external/typename.hpp"

void greet(double id, double score) {
  std::cout << "Hello from ID " << id << ", score: " << score << "\n";
}

int main() {
  arl::am_internal::AmHandlerRegistry registry;

  // Register function under ID 42
  uint8_t id = registry.register_amhandler(greet);

  // Prepare buffer with args
  char buffer[64];
  char* buf_ptr = buffer;
  arl::am_internal::AmHandlerRegistry::serialize_args(buf_ptr, 123.0, 98.6);

  // Later... dispatch using ID and buffer
  registry.invoke(id, buffer);
}