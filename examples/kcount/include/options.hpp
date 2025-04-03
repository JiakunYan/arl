#ifndef __OPTIONS_H
#define __OPTIONS_H

#include <iostream>
#include <regex>
#include <sys/stat.h>

#include "arl.hpp"
#include "external/cxxopts.hpp"

using std::cout;
using std::endl;
using std::vector;

#define YES_NO(X) ((X) ? "YES" : "NO")

struct Options {

  std::vector<string> reads_fname_list;
  int num_workers = 16;
  int num_threads = 16;
  int kmer_len = 51;
  bool verbose = false;
//  int max_kmer_store_mb = 50;
  int depth_thres = 2;
  bool show_progress = false;
  string output_fname = "";

  bool load(int argc, char **argv) {
    string reads_fnames;
    cxxopts::Options options("MHM (version) ", string(MHM_VERSION));
    options.add_options()("n,nworkers", "worker thread number", cxxopts::value<int>()->default_value("15"))("m,nthreads", "total thread number", cxxopts::value<int>()->default_value("16"))("r,reads", "Files containing reads in FASTQ format (comma separated)", cxxopts::value<string>())("k,kmer-len", "kmer length", cxxopts::value<int>()->default_value("51"))("p,progress", "Show progress", cxxopts::value<bool>()->default_value("false"))("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))("o", "Output name for kmers file (no output if not specified)", cxxopts::value<string>()->default_value(""));
    auto result = options.parse(argc, argv);
    try {
      num_workers = result["nworkers"].as<int>();
      num_threads = result["nthreads"].as<int>();
      reads_fnames = result["reads"].as<string>();
      kmer_len = result["kmer-len"].as<int>();
      show_progress = result["progress"].as<bool>();
      verbose = result["verbose"].as<bool>();
      output_fname = result["o"].as<string>();
    } catch (...) {
      printf("%s", options.help().c_str());
      return false;
    }
    reads_fname_list = split(reads_fnames, ',');

    if (show_progress) verbose = true;

    return true;
  }

  void print() {
    // print out all compiler definitions
    SLOG_VERBOSE(KBLUE, "_________________________\nCompiler definitions:", KNORM, "\n");
    std::istringstream all_defs_ss(ALL_DEFNS);
    vector<string> all_defs((std::istream_iterator<string>(all_defs_ss)), std::istream_iterator<string>());
    for (auto &def : all_defs) SLOG_VERBOSE(KBLUE, "  ", def, KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "_________________________", KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "MHM options:", KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  reads files:           ", KNORM);
    for (auto read_fname : reads_fname_list) SLOG_VERBOSE(KLBLUE, read_fname, ",", KNORM);
    SLOG_VERBOSE("\n");
    SLOG_VERBOSE(KLBLUE, "  Worker thread number:  ", num_workers, KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  Total thread number:   ", num_threads, KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  kmer length:           ", kmer_len, KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  depth threshold:       ", depth_thres, KNORM, "\n");
    if (!output_fname.empty()) SLOG_VERBOSE(KLBLUE, "  output file:           ", output_fname, KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  show progress:         ", YES_NO(show_progress), KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "  verbose:               ", YES_NO(verbose), KNORM, "\n");
    SLOG_VERBOSE(KLBLUE, "_________________________", KNORM, "\n");

    double start_mem_free = get_free_mem_gb();
    SLOG_VERBOSE("Initial free memory on node 0: ", std::setprecision(3), std::fixed, start_mem_free, " GB\n");
    SLOG_VERBOSE("Running on ", arl::proc::rank_n(), " ranks\n");
#ifdef DEBUG
    SWARN("Running low-performance debug mode", KNORM);
#endif
  }
};


#endif
