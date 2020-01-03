#ifndef __OPTIONS_H
#define __OPTIONS_H

#include <iostream>
#include <regex>
#include <sys/stat.h>
#include <upcxx/upcxx.hpp>

#include "utils.hpp"
#include "CLI11.hpp"

using std::cout;
using std::endl;
using std::vector;

#define YES_NO(X) ((X) ? "YES" : "NO")

struct Options {

  vector<string> reads_fname_list;
  int kmer_len = 51;
  bool verbose = false;
  int max_kmer_store_mb = 50;
  int depth_thres = 2;
  bool show_progress = false;
  string output_fname = "";

  bool load(int argc, char **argv) {
    string reads_fnames;

    //SLOG(KBLUE, "MHM version ", MHM_VERSION, KNORM, "\n");
    CLI::App app("MHM (version) " + string(MHM_VERSION));

    app.add_option("-r, --reads", reads_fnames, "Files containing reads in FASTQ format (comma separated)") -> required();
    app.add_option("-k, --kmer-len", kmer_len, "kmer length") -> required();
    app.add_option("-s,--max-kmer-store", max_kmer_store_mb,
                   "Max size (in MB) for kmer store (default " + to_string(max_kmer_store_mb) + ")");
    app.add_option("-o", output_fname, "Output name for kmers file (no output if not specified)");
    app.add_option("-d, depth-thres", depth_thres,
                   "Depth threshold for keeping kmers (default " + to_string(depth_thres) + ")");
    app.add_flag("-p,--progress", show_progress, "Show progress");
    app.add_flag("-v, --verbose", verbose, "Verbose output");

    try {
      app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
      if (upcxx::rank_me() == 0) app.exit(e);
      return false;
    }

    reads_fname_list = split(reads_fnames, ',');
    if (show_progress) verbose = true;

    if (upcxx::rank_me() == 0) {
      // print out all compiler definitions
      SLOG(KBLUE, "_________________________\nCompiler definitions:", KNORM, "\n");
      std::istringstream all_defs_ss(ALL_DEFNS);
      vector<string> all_defs((std::istream_iterator<string>(all_defs_ss)), std::istream_iterator<string>());
      for (auto &def : all_defs) SLOG(KBLUE, "  ", def, KNORM, "\n");
      SLOG(KLBLUE, "_________________________", KNORM, "\n");
      SLOG(KLBLUE, "MHM options:", KNORM, "\n");
      SLOG(KLBLUE, "  reads files:           ", KNORM);
      for (auto read_fname : reads_fname_list) SLOG(KLBLUE, read_fname, ",", KNORM);
      SLOG("\n");
      SLOG(KLBLUE, "  kmer length:           ", kmer_len, KNORM, "\n");
      SLOG(KLBLUE, "  max kmer store:        ", max_kmer_store_mb, KNORM, " MB\n");
      SLOG(KLBLUE, "  depth threshold:       ", depth_thres, KNORM, "\n");
      if (!output_fname.empty()) SLOG(KLBLUE, "  output file:           ", output_fname, KNORM, "\n");
      SLOG(KLBLUE, "  show progress:         ", YES_NO(show_progress), KNORM, "\n");
      SLOG(KLBLUE, "  verbose:               ", YES_NO(verbose), KNORM, "\n");
      SLOG(KLBLUE, "_________________________", KNORM, "\n");

      double start_mem_free = get_free_mem_gb();
      SLOG("Initial free memory on node 0: ", std::setprecision(3), std::fixed, start_mem_free, " GB\n");
      SLOG("Running on ", upcxx::rank_n(), " ranks\n");
#ifdef DEBUG
      SWARN("Running low-performance debug mode", KNORM);
#endif
    }
    upcxx::barrier();
    return true;
  }
};


#endif
