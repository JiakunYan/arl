#ifndef __KMER_DHT
#define __KMER_DHT

#include <limits>
#include <iostream>
#include <map>
#include <fstream>
#include <chrono>
#include <stdarg.h>
#include <arl/arl.hpp>

#include "progressbar.hpp"
#include "utils.hpp"
#include "kmer.hpp"
#include "bloom.hpp"
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


//#define DBG_INS_CTG_KMER DBG
#define DBG_INS_CTG_KMER(...)
//#define DBG_INSERT_KMER DBG
#define DBG_INSERT_KMER(...)


enum PASS_TYPE { BLOOM_SET_PASS, BLOOM_COUNT_PASS };

// we use a struct here because that makes it easy to add additional fields, depending on the application
struct KmerCounts {
  int64_t count;
};

using KmerMap = HashMap<Kmer, KmerCounts, KmerHash, KmerEqual>;

class KmerDHT {

  // instead of this structure, we could just use a MerArray, since the count is always one. However, this is kept as a
  // placeholder in case more complex metadata about a kmer is going to be added in future
  struct MerarrAndCount {
    MerArray merarr;
    int64_t count;
  };

  KmerMap kmers;
  // The first bloom filter stores all kmers and is used to check for single occurrences to filter out
  dist_object<BloomFilter> bloom_filter_singletons;
  // the second bloom filer stores only kmers that are above the repeat depth, and is used for sizing the kmer hash table
  dist_object<BloomFilter> bloom_filter_repeats;
  int64_t max_kmer_store_bytes;
  int64_t initial_kmer_dht_reservation;
  int64_t bloom1_cardinality;

  struct InsertKmerInBloom {
    void operator()(MerArray &merarr, dist_object<BloomFilter> &bloom_filter_singletons,
                    dist_object<BloomFilter> &bloom_filter_repeats) {
      // look for it in the first bloom fililtter - if not found, add it just to the first bloom filter
      // if found, add it to the second bloom filter
      Kmer new_kmer(merarr);
      if (!bloom_filter_singletons->possibly_contains(new_kmer.get_bytes(), new_kmer.get_num_bytes()))
        bloom_filter_singletons->add(new_kmer.get_bytes(), new_kmer.get_num_bytes());
      else
        bloom_filter_repeats->add(new_kmer.get_bytes(), new_kmer.get_num_bytes());
    }
  };
  dist_object<InsertKmerInBloom> insert_kmer_in_bloom;

  struct InsertKmer {
    void operator()(MerarrAndCount &merarr_and_count, dist_object<KmerMap> &kmers, dist_object<BloomFilter> &bloom_filter) {
      Kmer new_kmer(merarr_and_count.merarr);
      // if the kmer is not found in the bloom filter, skip it
      if (!bloom_filter->possibly_contains(new_kmer.get_bytes(), new_kmer.get_num_bytes())) return;
      // add or update the kmer count
      const auto it = kmers->find(new_kmer);
      if (it == kmers->end()) {
        KmerCounts kmer_counts = {.count = 1};
        auto prev_bucket_count = kmers->bucket_count();
        kmers->insert({new_kmer, kmer_counts});
        // this shouldn't happen
        if (prev_bucket_count < kmers->bucket_count()) {
          // actually, this should not happen
          WARN("Hash table on rank 0 was resized from ", prev_bucket_count, " to ", kmers->bucket_count(), "\n");
        }
      } else {
        // increment the count
        it->second.count++;
      }
    }
  };
  dist_object<InsertKmer> insert_kmer;

  size_t get_kmer_target_rank(Kmer &kmer) {
    return std::hash<Kmer>{}(kmer) % rank_n();
  }

public:

  KmerDHT(uint64_t cardinality, int max_kmer_store_bytes)
    : kmers({})
    , bloom_filter_singletons({})
    , bloom_filter_repeats({})
    , kmer_store({})
    , kmer_store_bloom({})
    , insert_kmer_in_bloom({})
    , insert_kmer({})
    , max_kmer_store_bytes(max_kmer_store_bytes)
    , initial_kmer_dht_reservation(0)
    , bloom1_cardinality(0) {

    // main purpose of the timer here is to track memory usage
    Timer timer(__func__);
    kmer_store_bloom.set_size("bloom", max_kmer_store_bytes);
    // in this case we get an accurate estimate of the hash table size after the first bloom round, so the hash table
    // space is reserved then
    double init_mem_free = get_free_mem_gb();
    bloom_filter_singletons->init(cardinality, BLOOM_FP);
    // second bloom will have far fewer entries - assume 75% are filtered out
    bloom_filter_repeats->init(cardinality / 4, BLOOM_FP);
    SLOG_VERBOSE("Bloom filters used ", (init_mem_free - get_free_mem_gb()), "GB memory on node 0\n");
  }

  void clear() {
    kmers->clear();
    KmerMap().swap(*kmers);
  }

  ~KmerDHT() {
    kmer_store_bloom.clear();
    kmer_store.clear();
    clear();
  }

  int64_t get_num_kmers(bool all = false) {
    if (!all) return reduce_one(kmers->size(), op_fast_add, 0).wait();
    else return reduce_all(kmers->size(), op_fast_add).wait();
  }

  float max_load_factor() {
    return reduce_one(kmers->max_load_factor(), op_fast_max, 0).wait();
  }

  float load_factor() {
    int64_t cardinality = initial_kmer_dht_reservation * rank_n();
    int64_t num_kmers = get_num_kmers();
    SLOG_VERBOSE("Originally reserved ", cardinality, " and now have ", num_kmers, " elements\n");
    return reduce_one(kmers->load_factor(), op_fast_add, 0).wait() / upcxx::rank_n();
  }

  KmerCounts *get_local_kmer_counts(Kmer &kmer) {
    const auto it = kmers->find(kmer);
    if (it == kmers->end()) return nullptr;
    return &it->second;
  }

  KmerCounts get_kmer_counts(Kmer &kmer) {
    return rpc(get_kmer_target_rank(kmer),
               [](MerArray merarr, dist_object<KmerMap> &kmers) -> KmerCounts {
                 Kmer kmer(merarr);
                 const auto it = kmers->find(kmer);
                 if (it == kmers->end()) return {.count = 0};
                 else return it->second;
               }, kmer.get_array(), kmers).wait();
  }

  void add_kmer(Kmer kmer, PASS_TYPE pass_type) {
    // get the lexicographically smallest
    Kmer kmer_rc = kmer.revcomp();
    if (kmer_rc < kmer) kmer = kmer_rc;
    auto target_rank = get_kmer_target_rank(kmer);
    MerarrAndCount merarr_and_count = { kmer.get_array(), 1 };
    switch (pass_type) {
      case BLOOM_SET_PASS:
        kmer_store_bloom.update(target_rank, merarr_and_count.merarr, insert_kmer_in_bloom, bloom_filter_singletons, bloom_filter_repeats);
        break;
      case BLOOM_COUNT_PASS:
        kmer_store.update(target_rank, merarr_and_count, insert_kmer, kmers, bloom_filter_repeats);
        break;
    };
  }

  void reserve_space_and_clear_bloom() {
    Timer timer(__func__);
    // at this point we're done with generating the bloom filters, so we can drop the first bloom filter and
    // allocate the kmer hash table

    // purge the kmer store and prep the kmer + count
    kmer_store_bloom.clear();
    kmer_store.set_size("kmers", max_kmer_store_bytes);

    int64_t cardinality1 = bloom_filter_singletons->estimate_num_items();
    int64_t cardinality2 = bloom_filter_repeats->estimate_num_items();
    bloom1_cardinality = cardinality1;
    SLOG_VERBOSE("Rank 0: first bloom filter size estimate is ", cardinality1, " and second size estimate is ",
                 cardinality2, " ratio is ", (double)cardinality2 / cardinality1, "\n");
    bloom_filter_singletons->clear(); // no longer need it

    double init_mem_free = get_free_mem_gb();
    barrier();
    // two bloom false positive rates applied
    initial_kmer_dht_reservation = (int64_t) (cardinality2 * (1+BLOOM_FP) * (1+BLOOM_FP) + 1000);
    kmers->reserve( initial_kmer_dht_reservation );
    double kmers_space_reserved = initial_kmer_dht_reservation * (sizeof(Kmer) + sizeof(KmerCounts));
    SLOG_VERBOSE("Rank 0 is reserving ", get_size_str(kmers_space_reserved), " for kmer hash table with ",
                 initial_kmer_dht_reservation, " entries (", kmers->bucket_count(), " buckets)\n");
    barrier();
  }

  void flush_updates(PASS_TYPE pass_type) {
    Timer timer(__func__);
    barrier();
    if (pass_type == BLOOM_SET_PASS) kmer_store_bloom.flush_updates(insert_kmer_in_bloom, bloom_filter_singletons, bloom_filter_repeats);
    else if (pass_type == BLOOM_COUNT_PASS) kmer_store.flush_updates(insert_kmer, kmers, bloom_filter_repeats);
    else DIE("bad pass type\n");
    barrier();
  }

  void purge_kmers(int threshold) {
    Timer timer(__func__);
    auto num_prior_kmers = get_num_kmers();
    int64_t num_purged = 0;
    for (auto it = kmers->begin(); it != kmers->end(); ) {
      if (it->second.count < threshold) {
        num_purged++;
        it = kmers->erase(it);
      } else {
        ++it;
      }
    }
    auto all_num_purged = reduce_one(num_purged, op_fast_add, 0).wait();
    SLOG_VERBOSE("Purged ", perc_str(all_num_purged, num_prior_kmers), " kmers below frequency threshold of ",
                 threshold, "\n");
  }

  // one line per kmer, format:
  // KMERCHARS COUNT
  void dump_kmers(const string &fname, int k) {
    Timer timer(__func__);
    string dump_fname = fname + "-" + to_string(k) + ".kmers.gz";
    get_rank_path(dump_fname, rank_me());
    zstr::ofstream dump_file(dump_fname);
    ostringstream out_buf;
    ProgressBar progbar(kmers->size(), "Dumping kmers to " + dump_fname);
    int64_t i = 0;
    for (auto &elem : *kmers) {
      out_buf << elem.first << " " << elem.second.count << endl;
      i++;
      if (!(i % 1000)) {
        dump_file << out_buf.str();
        out_buf = ostringstream();
      }
      progbar.update();
    }
    if (!out_buf.str().empty()) dump_file << out_buf.str();
    dump_file.close();
    progbar.done();
    SLOG_VERBOSE("Dumped ", this->get_num_kmers(), " kmers\n");
  }

};

#endif
