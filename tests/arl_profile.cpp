#define ARL_DEBUG
#include "arl/arl.hpp"

void worker() {
  arl::print("The number of processors configured is %ld\n", sysconf(_SC_NPROCESSORS_CONF));
//  arl::print("Size of arl::rpc_t %lu\n", sizeof(arl::rpc_t));
//  arl::print("Size of arl::rpc_t::rpc_result_t %lu\n", sizeof(arl::rpc_t::rpc_result_t));
//  arl::print("Size of arl::rpc_t::payload_t %lu\n", sizeof(arl::rpc_t::payload_t));
//  arl::print("Size of arl::rpc_t::rpc_result_t::payload_t %lu\n", sizeof(arl::rpc_t::rpc_result_t::payload_t));
//  arl::print("maximum aggregation size = %lu\n", arl::get_max_agg_size());
  arl::print("Aggregation size is %lu Bytes\n", arl::get_agg_size());
}

int main(int argc, char** argv) {
  // one process per node
  arl::init();
  arl::run(worker);

  arl::finalize();
}
