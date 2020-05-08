# ARL Changelog

## [0.3.0]
### New
- add vector-version reduce_{one,all}.

### Changed
- improve performance of rpc_{agg, aggrd}: use thread-local counter.
- improve performance of rpc_{ff, ffrd}: remove GASNet reply message but use a collective reduce to flush all rpcs.

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