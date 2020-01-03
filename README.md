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
  printf("Worker %lu/%lu is running an RPC from worker %lu!\n", my_worker(), nworkers(), source);
  return my_worker();
}

void worker(int arg) {
  rank_t target = (my_worker() + 1) % nworkers();
  auto future = rpc(target, say_hello, my_worker());
  rank_t result = future.get();
  printf("Worker %lu/%lu's RPC has been executed by worker %lu!\n", my_worker(), nworkers(), result);
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
ARL is previously developed at [berkeley-container-library/bcl/branch-arh](https://github.com/berkeley-container-library/bcl/tree/arh/bcl/containers/experimental/arh).