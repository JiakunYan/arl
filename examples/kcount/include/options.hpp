#ifndef __OPTIONS_H
#define __OPTIONS_H

#include <iostream>
#include <regex>
#include <sys/stat.h>

#include "arl/arl.hpp"
#include "external/cxxopts.hpp"

using std::cout;
using std::endl;
using std::vector;

#define YES_NO(X) ((X) ? "YES" : "NO")

struct Options {

  std::vector<string> reads_fname_list;
  int kmer_len = 51;
  bool verbose = false;
//  int max_kmer_store_mb = 50;
  int depth_thres = 2;
  bool show_progress = false;
  string output_fname = "";

  bool load(int argc, char **argv) {
    string reads_fnames;
    cxxopts::Options options("MHM (version) ", string(MHM_VERSION));
    options.add_options()
            ("r,reads", "Files containing reads in FASTQ format (comma separated)", cxxopts::value<string>())
            ("k,kmer-len", "kmer length", cxxopts::value<int>()->default_value("51"))
            ("p,progress", "Show progress", cxxopts::value<bool>()->default_value("false"))
            ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
            ("o", "Output name for kmers file (no output if not specified)", cxxopts::value<string>()->default_value(""))
            ;
    auto result = options.parse(argc, argv);
    try {
      reads_fnames = result["reads"].as<string>();
      kmer_len = result["kmer-len"].as<int>();
      show_progress = result["progress"].as<bool>();
      verbose = result["verbose"].as<bool>();
      output_fname = result["o"].as<string>();
    } catch (...) {
      if (arl::proc::rank_me() == 0) {
        printf("%s", options.help().c_str());
      }
      return false;
    }
    reads_fname_list = split(reads_fnames, ',');

    if (show_progress) verbose = true;

    if (arl::proc::rank_me() == 0) {
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
      SLOG(KLBLUE, "  depth threshold:       ", depth_thres, KNORM, "\n");
      if (!output_fname.empty()) SLOG(KLBLUE, "  output file:           ", output_fname, KNORM, "\n");
      SLOG(KLBLUE, "  show progress:         ", YES_NO(show_progress), KNORM, "\n");
      SLOG(KLBLUE, "  verbose:               ", YES_NO(verbose), KNORM, "\n");
      SLOG(KLBLUE, "_________________________", KNORM, "\n");

      double start_mem_free = get_free_mem_gb();
      SLOG("Initial free memory on node 0: ", std::setprecision(3), std::fixed, start_mem_free, " GB\n");
      SLOG("Running on ", arl::proc::rank_n(), " ranks\n");
#ifdef DEBUG
      SWARN("Running low-performance debug mode", KNORM);
#endif
    }
    arl::proc::barrier();
    return true;
  }
};


#endif
