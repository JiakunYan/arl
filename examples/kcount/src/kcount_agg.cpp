// kcount - kmer counting
// Steven Hofmeyr, LBNL, June 2019
#include <iostream>
#include <math.h>
#include <algorithm>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#include "arl.hpp"
#include "kcount_config.hpp"
#include "fastq.hpp"
#include "kmer_dht_arl.hpp"
#include "options.hpp"
#include "progressbar.hpp"
#include "utils.hpp"

using namespace std;
using namespace arl;

bool _verbose = false;
bool _show_progress = false;

unsigned int Kmer::k = 0;
using options_t = decltype(make_shared<Options>());

uint64_t estimate_num_kmers(unsigned kmer_len, vector<string> &reads_fname_list) {
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
    progbar.done(true);
    barrier();
  }
  double fraction = (double) total_records_processed / (double) estimated_total_records;
//  DBG("This rank processed ", num_lines, " lines (", num_reads, " reads), and found ", num_kmers, " kmers\n");
  auto all_num_lines = reduce_one(num_lines / fraction, op_plus(), 0);
  auto all_num_reads = reduce_one(num_reads / fraction, op_plus(), 0);
  auto all_num_kmers = reduce_one(num_kmers / fraction, op_plus(), 0);
  int percent = 100.0 * fraction;
  SLOG_VERBOSE_ALL("Processed ", percent, " % of the estimated total of ", (int64_t)all_num_lines,
               " lines (", (int64_t)all_num_reads, " reads), and found an estimated maximum of ",
               (int64_t)all_num_kmers, " kmers\n");
  return num_kmers / fraction;
}

static void count_kmers(unsigned kmer_len, vector<string> &reads_fname_list, KmerDHT &kmer_dht,
                        PASS_TYPE pass_type) {
  switch (pass_type) {
    case BLOOM_SET_PASS:
      kmer_dht.register_add_kmer_set_aggrd();
      break;
    case BLOOM_COUNT_PASS:
      kmer_dht.register_add_kmer_count_aggrd();
      break;
  }
  SimpleTimer timer_total;
  int64_t num_reads = 0;
  int64_t num_lines = 0;
  int64_t num_kmers = 0;
  string progbar_prefix = "";
  switch (pass_type) {
    case BLOOM_SET_PASS: progbar_prefix = "Pass 1: Parsing reads file to setup bloom filter"; break;
    case BLOOM_COUNT_PASS: progbar_prefix = "Pass 2: Parsing reads file to count kmers"; break;
  };
  vector<Future<void>> futures;
  //char special = qual_offset + 2;
  timer_total.start();
  for (auto const &reads_fname : reads_fname_list) {
    FastqReader fqr(reads_fname, false);
    string id, seq, quals;
    ProgressBar progbar(fqr.my_file_size(), progbar_prefix);
    size_t tot_bytes_read = 0;
    while (true) {
      size_t bytes_read = fqr.get_next_fq_record(id, seq, quals);
      if (!bytes_read) break;
      num_lines += 4;
      num_reads++;
      tot_bytes_read += bytes_read;
      progbar.update(tot_bytes_read);
      if (seq.length() < kmer_len) continue;
      // split into kmers
      auto kmers = Kmer::get_kmers(kmer_len, seq);
      for (int i = 1; i < kmers.size() - 1; i++) {
        Future<void> future;
        switch (pass_type) {
          case BLOOM_SET_PASS:
            future = kmer_dht.add_kmer_set_aggrd(kmers[i]);
            break;
          case BLOOM_COUNT_PASS:
            future = kmer_dht.add_kmer_count_aggrd(kmers[i]);
            break;
        }
        futures.push_back(move(future));
        num_kmers++;
      }
      progress_external();
    }
    progbar.done(true);
    barrier();
    for (const auto& future: futures) {
      future.get();
    }
  }
  timer_total.end();
//  DBG("This rank processed ", num_lines, " lines (", num_reads, " reads)\n");
  auto all_num_lines = reduce_one(num_lines, op_plus(), 0);
  auto all_num_reads = reduce_one(num_reads, op_plus(), 0);
  auto all_num_kmers = reduce_one(num_kmers, op_plus(), 0);
  auto all_distinct_kmers = kmer_dht.size();
  SLOG_VERBOSE_ALL("Processed a total of ", all_num_lines, " lines (", all_num_reads, " reads)\n");
  SLOG_VERBOSE_ALL("Processed a total of ", all_num_kmers, " kmers (", timer_total.to_s(), " s)\n");
  SLOG_VERBOSE_ALL("Estimated overhead is ", timer_total.to_us() / num_kmers, " us\n");
  SLOG_VERBOSE_ALL("Estimated node bandwidth is ", (double) all_num_kmers * sizeof(Kmer) / timer_total.to_s() / 1e6 / proc::rank_n() * 2, " MB/s\n");
  if (pass_type != BLOOM_SET_PASS) SLOG_ALL("Found ", perc_str(all_distinct_kmers, all_num_kmers), " unique kmers\n");
}
//
void worker(const options_t& options) {
  auto start_t = chrono::high_resolution_clock::now();

  // get total file size across all libraries
  double tot_file_size = 0;
  if (!rank_me()) {
    for (auto const &reads_fname : options->reads_fname_list) tot_file_size += get_file_size(reads_fname);
    SLOG("Total size of ", options->reads_fname_list.size(), " input file", (options->reads_fname_list.size() > 1 ? "s" : ""),
         " is ", get_size_str(tot_file_size), "\n");
  }
  uint64_t my_num_kmers = estimate_num_kmers(options->kmer_len, options->reads_fname_list);
  KmerDHT kmer_dht(my_num_kmers);
  barrier();
  count_kmers(options->kmer_len, options->reads_fname_list, kmer_dht, BLOOM_SET_PASS);
  kmer_dht.reserve_space_and_clear_bloom();
  SLOG_VERBOSE_ALL("Reserve space for ", kmer_dht.capacity(), " kmers\n");
  barrier();
  count_kmers(options->kmer_len, options->reads_fname_list, kmer_dht, BLOOM_COUNT_PASS);
  barrier();
  SLOG_VERBOSE_ALL("After counting, there are ", kmer_dht.size(), " kmers; capacity:", kmer_dht.capacity(), "\n");
  SLOG_VERBOSE_ALL("kmer DHT load factor: ", kmer_dht.load_factor(), "\n");
  barrier();
  kmer_dht.purge_kmers(options->depth_thres);
  SLOG_VERBOSE_ALL("After purge of kmers < ", options->depth_thres, " there are ", kmer_dht.size(), " unique kmers\n");
  barrier();
  chrono::duration<double> t_elapsed = chrono::high_resolution_clock::now() - start_t;
  SLOG_ALL("Finished in ", setprecision(2), fixed, t_elapsed.count(), " s at ", get_current_time(), "\n");
  if (!options->output_fname.empty()) {
    kmer_dht.dump_kmers(options->output_fname, options->kmer_len);
    SLOG_VERBOSE_ALL("Dumped ", kmer_dht.size(), " kmers\n");
  }
}

int main(int argc, char **argv) {
  auto options = make_shared<Options>();
  if (!options->load(argc, argv)) return 0;
  _show_progress = options->show_progress;
  _verbose = options->verbose;
  Kmer::k = options->kmer_len;

  init(options->num_workers, options->num_threads);
  if (proc::rank_me() == 0) options->print();

  run(worker, options);
  finalize();
  return 0;
}
