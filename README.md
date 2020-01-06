# ARL

[![Build Status](https://travis-ci.com/JiakunYan/arl.svg?branch=master)](https://travis-ci.com/JiakunYan/arl)

The Asynchronous RPC Library is a C++ library supporting high-throughput asynchronous
Remote Procedure Call (RPC) requesting and execution. It is powered by:

- Node-level Aggregation
- Single-node Work-stealing

## Example

```cpp
#include "arl/arl.hpp"

using namespace arl;

rank_t say_hello(rank_t source) {
  printf("Worker %lu/%lu is running an RPC from worker %lu!\n", rank_me(), rank_n(), source);
  return rank_me();
}

void worker(int arg) {
  rank_t target = (rank_me() + 1) % rank_n();
  auto future = rpc(target, say_hello, rank_me());
  rank_t result = future.get();
  printf("Worker %lu/%lu's RPC has been executed by worker %lu!\n", rank_me(), rank_n(), result);
}

int main(int argc, char** argv) {
  // one process per node/processor
  arl::init(15, 16); // 15 worker, 1 progress thread per process
  arl::run(worker, 233); // you can pass some arguments if you want
  arl::finalize();
}
```

## Bug
Please let us know if you identify any bugs or general usability issues by creating
an issue on GitHub or directly contacting the authors.

## Note
ARL was previously developed at [berkeley-container-library/bcl/branch-arh](https://github.com/berkeley-container-library/bcl/tree/arh/bcl/containers/experimental/arh).
