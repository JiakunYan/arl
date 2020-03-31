# ARL Changelog

## [0.2.0] - 2019-03-31

### New features
- Enable reduction on AMs with same function handler: register_{aggrd, ffrd}, rpc_{aggrd, ffrd}

### Changed
- Improve AM core: now rpc,rpc_{agg,aggrd,ff,ffrd} can accept arguments of arbitrary length (no more than gasnet_AMMaxMedium, 4KB-64KB on Cori)
- Add more AggBuffer options

## [0.1.0] - 2019-03-31
### New
- Thread management module: init, run, finalize
- AM module: rpc, rpc_agg, rpc_ff
- Collective communication: barrier, broadcast, reduce_one, reduce_all
- Data structure: HashMap, BloomFilter(local)
- Microbenchmark: bandwidth
- Benchmark: Kmer counting, Contig generation