//
// Created by Jiakun Yan on 1/14/20.
//

#ifndef KCOUNT_CONFIG_HPP
#define KCOUNT_CONFIG_HPP

#define MAX_KMER_LEN 160
//# quality encoding is logarithmic, so the probability of an error is:
//# cutoff 10: 10% chance
//# cutoff 20: 1% chance
//# cutoff 30: 0.1% chance
#define BLOOM_FP 0.05
#define MAX_FILE_PATH 255
#define MAX_RANKS_PER_DIR 1024
#define MAX_RPCS_IN_FLIGHT 8192
//# 4MB in flight
#define MIN_INFLIGHT_BYTES 4194304
#define USE_COLORS
#define USE_BYTELL
#define USE_PROGBAR
#define MHM_VERSION "ARL v0.5.0"
#define ALL_DEFNS "all defns"

#endif //KCOUNT_CONFIG_HPP
