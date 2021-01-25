# ARL Changelog

## [Unreleased]
### New
- cmake build system.
- spmv benchmark.
- deadlock_detector: provide timeout+traceback functionality to debug deadlock applications.
- networkInfo: make ARL runtime able to provide network information.
- dist_wrapper: a distributed data structure wrapper for traditional data structures

### Changed
- Refactor communication backend.
- AggBufferAtomic: optionally reserve prefix buffer space for metadata.

### Fix
- AggBufferAtomic: fix a bug when concurrently invoking push and flush

## [0.4.0]
### Changed
- rpc, rpc_{agg,ff,aggrd,ffrd}: move the execution of ARL handlers out of GASNet-EX handlers.
- split progress into progress_internal and progress_external

## [0.3.1] - 2020-05-08
### Changed
- reduce overhead of the rank system (rank_me, {set,get}_context): replace the hashmap with TLS variables.
- stop using the progress() function as the backoff function in AggBuffer. This should be able to make the overhead of AggBuffer independent of the handler size.

### Removed
- remove SharedTimer. It can be perfectly substituted by a TLS SimpleTimer.

## [0.3.0] - 2020-05-08
### New
- add vector-version reduce_{one,all}.

### Changed
- improve performance of rpc_{agg,aggrd}: use thread-local counter.
- improve performance of rpc_{ff,ffrd}: remove GASNet reply message but use a collective reduce to flush all rpcs.

## [0.2.0] - 2020-04-09
### New
- Enable reduction on AMs with same function handler: register_{aggrd, ffrd}, rpc_{aggrd, ffrd}

### Changed
- Improve AM core: now rpc,rpc_{agg,aggrd,ff,ffrd} can accept arguments of arbitrary length (no more than gasnet_AMMaxMedium, 4KB-64KB on Cori)
- Add more AggBuffer options

## [0.1.0] - 2020-03-31
### New
- Thread management module: init, run, finalize
- AM module: rpc, rpc_agg, rpc_ff
- Collective communication: barrier, broadcast, reduce_one, reduce_all
- Data structure: HashMap, BloomFilter(local)
- Microbenchmark: bandwidth
- Benchmark: Kmer counting, Contig generation