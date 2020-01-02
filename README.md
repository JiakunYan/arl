# ARL
The Asynchronous RPC Library is a C++ library supporting high-throughput asynchronous
Remote Procedure Call (RPC) requesting and execution with

- Node-level Aggregation
- Single-node Work-stealing

## Example

```cpp
  #include "arl/arl.hpp"

  using namespace arl;

  void worker(int val) {
    printf("Hello, I am thread %lu/%lu! receive arguments %d\n", my_worker(), nworkers(), val);
  }
  
  int main(int argc, char** argv) {
    // one process per processor
    init(15, 16); // 15 worker threads and 1 progress thread

    int val = 132; // arguments to be passed to threads
    run(worker, val);
  
    finalize();
  }
```

## Bugs
Please let us know if you identify any bugs or general usability issues by creating
an issue on GitHub or directly contacting the authors.