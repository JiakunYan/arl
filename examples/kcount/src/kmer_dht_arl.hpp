#ifndef __KMER_DHT
#define __KMER_DHT

#include <limits>
#include <iostream>
#include <map>
#include <fstream>
#include <chrono>
#include <stdarg.h>

#include "arl/arl.hpp"
#include "progressbar.hpp"
#include "kmer.hpp"
#include "zstr.hpp"

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
  int64_t initial_kmer_dht_reservation;

  size_t get_kmer_target_rank(Kmer &kmer) {
    return std::hash<Kmer>{}(kmer) % rank_n();
  }

public:

  KmerLHT(uint64_t cardinality)
    : kmers({})
    , bloom_filter_singletons(cardinality, BLOOM_FP)
    , bloom_filter_repeats(cardinality, BLOOM_FP)
    , initial_kmer_dht_reservation(0) {}

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
    if (!bloom_filter_singletons.possibly_contains(kmer))
      bloom_filter_singletons.add(kmer);
    else
      bloom_filter_repeats.add(kmer);
  }

  void add_kmer_count(Kmer kmer) {
    // if the kmer is not found in the bloom filter, skip it
    if (!bloom_filter_repeats.possibly_contains(kmer)) return;
    // add or update the kmer count
    KmerCounts kmer_counts = {.count = 1};
    kmers.upsert(
        kmer,
        [](KmerCounts &v) {
          v.count++;
        },
        kmer_counts);
  }

  void reserve_space_and_clear_bloom() {
    // at this point we're done with generating the bloom filters, so we can drop the first bloom filter and
    // allocate the kmer hash table

    int64_t cardinality1 = bloom_filter_singletons.estimate_num_items();
    int64_t cardinality2 = bloom_filter_repeats.estimate_num_items();
    bloom_filter_singletons.clear(); // no longer need it

    // two bloom false positive rates applied
    initial_kmer_dht_reservation = (int64_t) ((cardinality2 * (1+BLOOM_FP) * (1+BLOOM_FP) + 1000) * 1.5);
    kmers.reserve( initial_kmer_dht_reservation );
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

  size_t load_factor() const {
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

class KmerDHT {
private:
  std::vector<KmerLHT *> map_ptrs;
  // map the key to a target process
  int get_target_proc(const Kmer &kmer) {
    return std::hash<Kmer>{}(kmer) % map_ptrs.size();
  }
public:
  // initialize the local map
  KmerDHT(uint64_t cardinality) {
    map_ptrs.resize(proc::rank_n());
    if (local::rank_me() == 0) {
      map_ptrs[proc::rank_me()] = new KmerLHT(cardinality);
    }
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      broadcast(map_ptrs[i], i * local::rank_n());
    }
  }

  ~KmerDHT() {
    arl::barrier();
    if (local::rank_me() == 0) {
      delete map_ptrs[proc::rank_me()];
    }
  }

  void add_kmer_set(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_proc = get_target_proc(kmer);
    rpc_ff(target_proc * local::rank_n(),
               [](KmerLHT* lmap, Kmer kmer){
                 lmap->add_kmer_set(kmer);
               }, map_ptrs[target_proc], kmer);
  }

  void add_kmer_count(Kmer kmer) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    size_t target_proc = get_target_proc(kmer);
    rpc_ff(target_proc * local::rank_n(),
               [](KmerLHT* lmap, Kmer kmer){
                 lmap->add_kmer_count(kmer);
               }, map_ptrs[target_proc], kmer);
  }

  void reserve_space_and_clear_bloom() {
    if (local::rank_me() == 0) {
      map_ptrs[proc::rank_me()]->reserve_space_and_clear_bloom();
    }
  }

  int64_t purge_kmers(int threshold) {
    int64_t num_purged = 0;
    if (local::rank_me() == 0) {
      num_purged = map_ptrs[proc::rank_me()]->purge_kmers(threshold);
    }
    auto all_num_purged = reduce_all(num_purged, op_plus());
    return all_num_purged;
  }

  size_t size() const {
    size_t n = 0;
    if (local::rank_me() == 0) {
      n = map_ptrs[proc::rank_me()]->size();
    }
    return reduce_all(n, op_plus());
  }

  size_t load_factor() const {
    size_t factor = 0;
    if (local::rank_me() == 0) {
      factor = map_ptrs[proc::rank_me()]->load_factor();
    }
    return reduce_all(factor, op_plus()) / rank_n();
  }

  void dump_kmers(const string &fname, int k) {
    string dump_fname = fname + "-" + to_string(k) + ".kmers.gz";
    get_rank_path(dump_fname, proc::rank_me());
    if (local::rank_me() == 0) {
      map_ptrs[proc::rank_me()]->dump_kmers(dump_fname);
    }
  }
};

#endif
