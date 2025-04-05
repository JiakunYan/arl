#ifndef __KMER_DHT
#define __KMER_DHT

#include <limits>
#include <iostream>
#include <map>
#include <fstream>
#include <chrono>
#include <stdarg.h>

#include "arl.hpp"
#include "progressbar.hpp"
#include "kmer.hpp"
#include "zstr.hpp"

#include "external/libcuckoo/libcuckoo/cuckoohash_map.hh"

using std::vector;
using std::pair;
using std::ostream;
using std::ostringstream;
using std::sort;
using std::numeric_limits;
using std::make_shared;
using std::make_pair;
using std::shared_ptr;
using std::swap;
using std::array;
using std::endl;
using std::get;
using std::min;
using std::max;

using namespace arl;

enum PASS_TYPE { BLOOM_SET_PASS, BLOOM_COUNT_PASS };

// we use a struct here because that makes it easy to add additional fields, depending on the application
struct KmerCounts {
  int64_t count;
};

using KmerMap = libcuckoo::cuckoohash_map<Kmer, KmerCounts, KmerHash, KmerEqual>;

class KmerLHT {

  // instead of this structure, we could just use a MerArray, since the count is always one. However, this is kept as a
  // placeholder in case more complex metadata about a kmer is going to be added in future
  struct MerarrAndCount {
    MerArray merarr;
    int64_t count;
  };

  KmerMap kmers;
  // The first bloom filter stores all kmers and is used to check for single occurrences to filter out
  local::BloomFilter<Kmer> bloom_filter_singletons;
  // the second bloom filer stores only kmers that are above the repeat depth, and is used for sizing the kmer hash table
  local::BloomFilter<Kmer> bloom_filter_repeats;

  size_t get_kmer_target_rank(Kmer &kmer) {
    return std::hash<Kmer>{}(kmer) % rank_n();
  }

public:

  KmerLHT(uint64_t cardinality)
    : kmers({})
    , bloom_filter_singletons(cardinality, BLOOM_FP)
    , bloom_filter_repeats(cardinality, BLOOM_FP) {}

  void clear() {
    kmers.clear();
    KmerMap().swap(kmers);
  }

  ~KmerLHT() {
    clear();
  }

  KmerCounts get_kmer_counts(Kmer &kmer) {
    KmerCounts count{.count=0};
    bool success = kmers.find(kmer, count);
    return count;
  }

  void add_kmer_set(Kmer kmer) {
    if (bloom_filter_singletons.add(kmer))
      bloom_filter_repeats.add(kmer);
  }

  void add_kmer_count(Kmer kmer) {
    // if the kmer is not found in the bloom filter, skip it
    if (bloom_filter_repeats.possibly_contains(kmer)) {
      // add or update the kmer count
      KmerCounts kmer_counts = {.count = 1};
      kmers.upsert(
          kmer,
          [](KmerCounts &v) {
            v.count++;
          },
          kmer_counts);
    }
  }

  void reserve_space_and_clear_bloom() {
    // at this point we're done with generating the bloom filters, so we can drop the first bloom filter and
    // allocate the kmer hash table

    int64_t cardinality1 = bloom_filter_singletons.estimate_num_items();
    int64_t cardinality2 = bloom_filter_repeats.estimate_num_items();
    bloom_filter_singletons.clear(); // no longer need it

    // two bloom false positive rates applied
    auto initial_kmer_dht_reservation = (int64_t) ((cardinality2 * (1+BLOOM_FP) * (1+BLOOM_FP) + 1000) * 2);
    kmers.reserve( initial_kmer_dht_reservation * 2 );
    double kmers_space_reserved = initial_kmer_dht_reservation * (sizeof(Kmer) + sizeof(KmerCounts));
  }

  int64_t purge_kmers(int threshold) {
    int64_t num_purged = 0;
    auto locked_kmers = kmers.lock_table();
    for (auto it = locked_kmers.begin(); it != locked_kmers.end(); ) {
      if (it->second.count < threshold) {
        num_purged++;
        it = locked_kmers.erase(it);
      } else {
        ++it;
      }
    }
    return num_purged;
  }

  size_t size() const {
    return kmers.size();
  }

  size_t capacity() const {
    return kmers.capacity();
  }

  double load_factor() const {
    return kmers.load_factor();
  }

  // one line per kmer, format:
  // KMERCHARS COUNT
  void dump_kmers(const string &dump_fname) {
    zstr::ofstream dump_file(dump_fname);
    ostringstream out_buf;
    ProgressBar progbar(kmers.size(), "Dumping kmers to " + dump_fname);
    int64_t i = 0;
    auto locked_kmers = kmers.lock_table();
    for (auto it = locked_kmers.begin(); it != locked_kmers.end(); it++) {
      out_buf << it->first << " " << it->second.count << endl;
      i++;
      if (!(i % 1000)) {
        dump_file << out_buf.str();
        out_buf = ostringstream();
      }
      progbar.update();
    }
    if (!out_buf.str().empty()) dump_file << out_buf.str();
    dump_file.close();
    progbar.done(false);
  }
};

struct KmerDHT;
__thread KmerDHT *g_kmerDHT_singleton = nullptr;

class KmerDHT {
 public:
  // initialize the local map
  KmerDHT(uint64_t cardinality) {
    if (g_kmerDHT_singleton != nullptr) {
      cerr << "KmerDHT already initialized" << endl;
      abort();
    }
    g_kmerDHT_singleton = this;
    map_ptrs.resize(rank_n());
    map_ptrs[rank_me()] = new KmerLHT(cardinality);
    for (size_t i = 0; i < rank_n(); ++i) {
      broadcast(map_ptrs[i], i);
    }
  }

  ~KmerDHT() {
    g_kmerDHT_singleton = nullptr;
    arl::barrier();
    delete map_ptrs[rank_me()];
  }

  void register_add_kmer_set_aggrd() {
    register_amaggrd<decltype(kmer_set_fn), Kmer>(kmer_set_fn);
  }

  void register_add_kmer_count_aggrd() {
    register_amaggrd<decltype(kmer_count_fn), Kmer>(kmer_count_fn);
  }

  void register_add_kmer_set_ffrd() {
    register_amffrd(kmer_set_fn, Kmer());
  }

  void register_add_kmer_count_ffrd() {
    register_amffrd(kmer_count_fn, Kmer());
  }

  Future<void> add_kmer_set(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    return rpc_agg(target_rank, kmer_set_fn, kmer);
  }

  Future<void> add_kmer_count(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    return rpc_agg(target_rank, kmer_count_fn, kmer);
  }

  Future<void> add_kmer_set_aggrd(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    return rpc_aggrd(target_rank, kmer_set_fn, kmer);
  }

  Future<void> add_kmer_count_aggrd(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    return rpc_aggrd(target_rank, kmer_count_fn, kmer);
  }

  void add_kmer_set_ff(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    rpc_ff(target_rank, kmer_set_fn, kmer);
  }

  void add_kmer_count_ff(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    rpc_ff(target_rank, kmer_count_fn, kmer);
  }

  void add_kmer_set_ffrd(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    rpc_ffrd(target_rank, kmer_set_fn, kmer);
  }

  void add_kmer_count_ffrd(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_rank = get_target_rank(kmer, map_ptrs.size());
    rpc_ffrd(target_rank, kmer_count_fn, kmer);
  }

  void reserve_space_and_clear_bloom() {
    map_ptrs[rank_me()]->reserve_space_and_clear_bloom();
  }

  int64_t purge_kmers(int threshold) {
    int64_t num_purged = map_ptrs[rank_me()]->purge_kmers(threshold);
    auto all_num_purged = reduce_all(num_purged, op_plus());
    return all_num_purged;
  }

  size_t size() const {
    size_t n = map_ptrs[rank_me()]->size();
    return reduce_all(n, op_plus());
  }

  size_t capacity() const {
    size_t n = map_ptrs[rank_me()]->capacity();
    return reduce_all(n, op_plus());
  }

  double load_factor() const {
    double factor = map_ptrs[rank_me()]->load_factor();
    return reduce_all(factor, op_plus()) / rank_n();
  }

  void dump_kmers(const string &fname, int k) {
    string dump_fname = fname + "-" + to_string(k) + ".kmers.gz";
    get_rank_path(dump_fname, rank_me());
    map_ptrs[rank_me()]->dump_kmers(dump_fname);
  }

  // map the key to a target process
  static int get_target_rank(const Kmer &kmer, int nranks) {
    return std::hash<Kmer>{}(kmer) % nranks;
  }

  void kmer_set_real_fn(Kmer kmer) {
    int rank = get_target_rank(kmer, arl::rank_n());
    map_ptrs[rank_me()]->add_kmer_set(kmer);
  }

  void kmer_count_real_fn(Kmer kmer) {
    int rank = get_target_rank(kmer, arl::rank_n());
    map_ptrs[rank_me()]->add_kmer_count(kmer);
  }

 private:
  static constexpr auto kmer_set_fn = [](Kmer kmer){
    g_kmerDHT_singleton->kmer_set_real_fn(kmer);
  };

  static constexpr auto kmer_count_fn = [](Kmer kmer){
    g_kmerDHT_singleton->kmer_count_real_fn(kmer);
  };

  std::vector<KmerLHT *> map_ptrs;
};

#endif
