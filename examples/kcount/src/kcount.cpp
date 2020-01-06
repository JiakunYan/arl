// kcount - kmer counting
// Steven Hofmeyr, LBNL, June 2019

#include <iostream>
#include <math.h>
#include <algorithm>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "arl/arl.hpp"

#include "utils.hpp"
#include "progressbar.hpp"
#include "options.hpp"
#include "kmer_dht.hpp"
#include "fastq.hpp"

using namespace std;
using namespace arl;

//#define DBG_ADD_KMER DBG
#define DBG_ADD_KMER(...)

#ifndef NDEBUG
#define DEBUG
ofstream _dbgstream;
#endif
ofstream _logstream;

bool _verbose = false;
bool _show_progress = false;

unsigned int Kmer::k = 0;

using options_t = decltype(make_shared<Options>());

uint64_t estimate_num_kmers(unsigned kmer_len, vector<string> &reads_fname_list) {
  Timer timer(__func__);
  int64_t num_reads = 0;
  int64_t num_lines = 0;
  int64_t num_kmers = 0;
  int64_t estimated_total_records = 0;
  int64_t total_records_processed = 0;
  for (auto const &reads_fname : reads_fname_list) {
    FastqReader fqr(reads_fname, false);
    string id, seq, quals;
    ProgressBar progbar(fqr.my_file_size(), "Scanning reads file to estimate number of kmers");
    size_t tot_bytes_read = 0;
    int64_t records_processed = 0; 
    while (true) {
      size_t bytes_read = fqr.get_next_fq_record(id, seq, quals);
      if (!bytes_read) break;
      num_lines += 4;
      num_reads++;
      tot_bytes_read += bytes_read;
      progbar.update(tot_bytes_read);
      records_processed++;
      // do not read the entire data set for just an estimate
      if (records_processed > 100000) break; 
      if (seq.length() < kmer_len) continue;
      num_kmers += seq.length() - kmer_len + 1;
    }
    total_records_processed += records_processed;
    int64_t bytes_per_record = tot_bytes_read / records_processed;
    estimated_total_records += fqr.my_file_size() / bytes_per_record;
    progbar.done();
    barrier();
  }
  double fraction = (double) total_records_processed / (double) estimated_total_records;
  DBG("This rank processed ", num_lines, " lines (", num_reads, " reads), and found ", num_kmers, " kmers\n");
  auto all_num_lines = reduce_one(num_lines / fraction, op_plus(), 0);
  auto all_num_reads = reduce_one(num_reads / fraction, op_plus(), 0);
  auto all_num_kmers = reduce_all(num_kmers / fraction, op_plus());
  int percent = 100.0 * fraction;
  SLOG_VERBOSE("Processed ", percent, " % of the estimated total of ", (int64_t)all_num_lines,
               " lines (", (int64_t)all_num_reads, " reads), and found an estimated maximum of ", 
               (int64_t)all_num_kmers, " kmers\n");
  return num_kmers / fraction;
}

static void count_kmers(unsigned kmer_len, vector<string> &reads_fname_list, dist_object<KmerDHT> &kmer_dht, 
                        PASS_TYPE pass_type) {
  Timer timer(__func__);
  int64_t num_reads = 0;
  int64_t num_lines = 0;
  int64_t num_kmers = 0;
  string progbar_prefix = "";
  switch (pass_type) {
    case BLOOM_SET_PASS: progbar_prefix = "Pass 1: Parsing reads file to setup bloom filter"; break;
    case BLOOM_COUNT_PASS: progbar_prefix = "Pass 2: Parsing reads file to count kmers"; break;
  };
  IntermittentTimer read_io_timer("Read IO");
  //char special = qual_offset + 2;
  for (auto const &reads_fname : reads_fname_list) {
    FastqReader fqr(reads_fname, false);
    string id, seq, quals;
    ProgressBar progbar(fqr.my_file_size(), progbar_prefix);
    size_t tot_bytes_read = 0;
    while (true) {
      read_io_timer.start();
      size_t bytes_read = fqr.get_next_fq_record(id, seq, quals);
      read_io_timer.stop();
      if (!bytes_read) break;
      num_lines += 4;
      num_reads++;
      tot_bytes_read += bytes_read;
      progbar.update(tot_bytes_read);
      if (seq.length() < kmer_len) continue;
      // split into kmers
      auto kmers = Kmer::get_kmers(kmer_len, seq);
      for (int i = 1; i < kmers.size() - 1; i++) {
        kmer_dht->add_kmer(kmers[i], pass_type);
        num_kmers++;
      }
      progress();
    }
    progbar.done();
    kmer_dht->flush_updates(pass_type);
  }
  read_io_timer.done();
  DBG("This rank processed ", num_lines, " lines (", num_reads, " reads)\n");
  auto all_num_lines = reduce_one(num_lines, op_fast_add, 0).wait();
  auto all_num_reads = reduce_one(num_reads, op_fast_add, 0).wait();
  auto all_num_kmers = reduce_one(num_kmers, op_fast_add, 0).wait();
  auto all_distinct_kmers = kmer_dht->get_num_kmers();
  SLOG_VERBOSE("Processed a total of ", all_num_lines, " lines (", all_num_reads, " reads)\n");
  if (pass_type != BLOOM_SET_PASS) SLOG_VERBOSE("Found ", perc_str(all_distinct_kmers, all_num_kmers), " unique kmers\n");
}

void worker(const options_t& options) {
  init_logger();
  auto start_t = chrono::high_resolution_clock::now();
  double start_mem_free = get_free_mem_gb();

#ifdef DEBUG
  //time_t curr_t = std::time(nullptr);
  //string dbg_fname = "debug" + to_string(curr_t) + ".log";
  string dbg_fname = "debug.log";
  get_rank_path(dbg_fname, rank_me());
  _dbgstream.open(dbg_fname);
#endif

  // get total file size across all libraries
  double tot_file_size = 0;
  if (!rank_me()) {
    for (auto const &reads_fname : options->reads_fname_list) tot_file_size += get_file_size(reads_fname);
    SLOG("Total size of ", options->reads_fname_list.size(), " input file", (options->reads_fname_list.size() > 1 ? "s" : ""),
         " is ", get_size_str(tot_file_size), "\n");
  }
  auto my_num_kmers = estimate_num_kmers(options->kmer_len, options->reads_fname_list);
  dist_object<KmerDHT> kmer_dht(world(), my_num_kmers, options->max_kmer_store_mb * ONE_MB);
  barrier();
  count_kmers(options->kmer_len, options->reads_fname_list, kmer_dht, BLOOM_SET_PASS);
  kmer_dht->reserve_space_and_clear_bloom();
  count_kmers(options->kmer_len, options->reads_fname_list, kmer_dht, BLOOM_COUNT_PASS);
  barrier();
  SLOG_VERBOSE("kmer DHT load factor: ", kmer_dht->load_factor(), "\n");
  barrier();
  kmer_dht->purge_kmers(options->depth_thres);
  int64_t new_count = kmer_dht->get_num_kmers();
  SLOG_VERBOSE("After purge of kmers < ", options->depth_thres, " there are ", new_count, " unique kmers\n");
  barrier();
  chrono::duration<double> t_elapsed = chrono::high_resolution_clock::now() - start_t;
  SLOG("Finished in ", setprecision(2), fixed, t_elapsed.count(), " s at ", get_current_time(), "\n");
  if (!options->output_fname.empty()) kmer_dht->dump_kmers(options->output_fname, options->kmer_len);

#ifdef DEBUG
  _dbgstream.flush();
  _dbgstream.close();
#endif
}

int main(int argc, char **argv) {
  init_logger();
  double start_mem_free = get_free_mem_gb();

#ifdef DEBUG
  //time_t curr_t = std::time(nullptr);
  //string dbg_fname = "debug" + to_string(curr_t) + ".log";
  string dbg_fname = "debug.log";
  get_rank_path(dbg_fname, backend::rank_me());
  _dbgstream.open(dbg_fname);
#endif
  auto options = make_shared<Options>();
  if (!options->load(argc, argv)) return 0;
  set_logger_verbose(options->verbose);
  _show_progress = options->show_progress;
  Kmer::k = options->kmer_len;

  init(15, 16);
  run(worker, options);
  finalize();
  return 0;
}
