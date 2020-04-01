#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <set>
#include <numeric>
#include <cstddef>
#include <chrono>

#include "arl/arl.hpp"
#include "kmer_t.hpp"
#include "read_kmers.hpp"

using namespace arl;

class kmer_hash {
public:
  uint64_t operator()(const pkmer_t &kmer) const {
    return kmer.hash();
  }
};

std::string run_type = "";
std::string kmer_fname;

void worker(size_t n_kmers) {
  // Load factor of 0.5
  size_t hash_table_size = n_kmers * (1.0 / 0.5);
  arl::HashMap<pkmer_t, kmer_pair, kmer_hash> hashmap(hash_table_size);

  if (run_type == "verbose" || run_type == "verbose_test") {
    arl::print("agg_size: %lu, sizeof(pkmer_t): %lu, sizeof(kmer_pair): %lu.\n", arl::get_agg_size(), sizeof(pkmer_t), sizeof(kmer_pair));
    arl::print("Initializing hash table of size %d for %d kmers.\n",
                 hash_table_size, n_kmers);
  }

  std::vector <kmer_pair> kmers = read_kmers(kmer_fname, n_kmers, arl::rank_n(), arl::rank_me());

  if (run_type == "verbose" || run_type == "verbose_test") {
    arl::print("Finished reading kmers.\n");
  }

  auto start = ticks_now();

  std::vector <kmer_pair> start_nodes;
  hashmap.register_insert_ffrd();

  for (auto &kmer : kmers) {
    hashmap.insert_ffrd(kmer.kmer, kmer);
    if (kmer.backwardExt() == 'F') {
      start_nodes.push_back(kmer);
    }
  }

  auto end_insert = ticks_now();
  arl::barrier();

  double insert_time = ticks_to_s(end_insert-start);
  if (run_type != "test") {
    arl::print("Finished inserting in %lf\n", insert_time);
  }
  arl::barrier();

//------- Read Start -------

  auto start_read = ticks_now();
  hashmap.register_find_aggrd();

  std::list <std::list <kmer_pair>> contigs;
  struct task_t {
      arl::Future<kmer_pair> future;
      std::list <kmer_pair> contig;
  };
  std::list<task_t> taskPool;

  arl::barrier();
  for (const auto &start_kmer : start_nodes) {
    if (start_kmer.forwardExt() != 'F') {
      task_t task;
      task.contig.push_back(start_kmer);
      task.future = hashmap.find_aggrd(start_kmer.next_kmer());
      taskPool.push_back(std::move(task));
    } else {
      contigs.push_back(std::list<kmer_pair>({start_kmer}));
    }
  }

  while (!taskPool.empty()) {
    bool is_active = false;

    for (auto it = taskPool.begin(); it != taskPool.end();) {
      task_t& current_task = *it;

      if (current_task.future.ready()) {
        // current task is ready
        is_active = true;
        kmer_pair kmer = current_task.future.get();
        if (kmer == kmer_pair()) {
          throw std::runtime_error("Error: k-mer not found in hashmap.");
        }
        current_task.contig.push_back(kmer);

        if (kmer.forwardExt() != 'F') {
          // current task hasn't completed
          current_task.future = hashmap.find_aggrd(kmer.next_kmer());
          ++it;
        } else {
          // current task has completed
          contigs.push_back(std::move(current_task.contig));
          it = taskPool.erase(it);
        }
      } else {
        // current task is not ready
        ++it;
        arl::progress();
      }
    }

    if (!is_active) {
      // flush buffer
      arl::flush_agg_buffer();
//      arl::amaggrd_internal::flush_amaggrd();
      arl::flush_am();
    }
  }

  arl::barrier();

#ifdef DEBUG
  // only one thread arrive at Pos 2
  if (run_type == "verbose" || run_type == "verbose_test")
    printf("Pos 2 Rank %d\n", upcxx::rank_me());
#endif

  auto end = ticks_now();

  double read = ticks_to_s(end - start_read);
  double insert = ticks_to_s(end_insert - start);
  double total = ticks_to_s(end - start);

  int numKmers = std::accumulate(contigs.begin(), contigs.end(), 0,
                                 [] (int sum, const std::list <kmer_pair> &contig) {
                                     return sum + contig.size();
                                 });

  if (run_type != "test") {
    arl::print("Assembled in %lf total\n", total);
  }

  if (run_type == "verbose" || run_type == "verbose_test") {
    printf("Rank %ld reconstructed %lu contigs with %d nodes from %lu start nodes."
           " (%lf read, %lf insert, %lf total)\n", arl::rank_me(), contigs.size(),
           numKmers, start_nodes.size(), read, insert, total);
  }

  if (run_type == "test" || run_type == "verbose_test") {
    std::ofstream fout("test_" + std::to_string(arl::rank_me()) + ".dat");
    for (const auto &contig : contigs) {
      fout << extract_contig(contig) << std::endl;
    }
    fout.close();
  }

}

int main(int argc, char **argv) {
  arl::init(15, 16);
  if (argc < 2) {
    arl::proc::print("usage: srun -N nodes -n ranks ./kmer_hash kmer_file [verbose|test|verbose_test]\n");
    arl::finalize();
    exit(1);
  }

  kmer_fname = std::string(argv[1]);

  if (argc >= 3) {
    run_type = std::string(argv[2]);
  }

  int ks = kmer_size(kmer_fname);

  if (ks != KMER_LEN) {
    throw std::runtime_error("Error: " + kmer_fname + " contains " +
      std::to_string(ks) + "-mers, while this binary is compiled for " +
      std::to_string(KMER_LEN) + "-mers.  Modify packing.hpp and recompile.");
  }

  if (run_type == "verbose" || run_type == "verbose_test") {
    arl::proc::print("Start counting kmer number...\n");
  }
  size_t n_kmers = line_count(kmer_fname);
  if (run_type == "verbose" || run_type == "verbose_test") {
    arl::proc::print("Finish counting kmer number: %lu\n", n_kmers);
  }

  arl::run(worker, n_kmers);

  arl::finalize();
  return 0;
}