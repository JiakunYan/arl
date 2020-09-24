//
// Created by jiakun on 2020/8/28.
//
#define ARL_INFO
#include "arl/arl.hpp"
#include "csrmat/CSRMatrix.hpp"

using namespace arl;

using T = double;
using index_type = int;

bool is_test = false;
int total_steps = 100;
T get_value(std::vector<T>* local_v, index_type global_index) {
  index_type local_v_n = local_v->size();
  ARL_Assert(global_index >= local_v_n * rank_me());
  ARL_Assert(global_index < local_v_n * (rank_me() + 1));
  index_type local_index = global_index % local_v_n;
  return local_v->at(local_index);
}


void worker(const BCL::CSRMatrix<T, index_type>& global_mat) {

  size_t global_row_n = global_mat.shape()[0];
  size_t local_row_n = ceil((double) global_row_n / rank_n());
  size_t local_row_start = std::min(local_row_n * rank_me(), global_row_n);
  size_t local_row_end = std::min(local_row_n * (rank_me() + 1), global_row_n);
  size_t global_col_n = global_mat.shape()[1];

  BCL::CSRMatrix<T, index_type> local_mat =
      global_mat.get_slice_impl_(local_row_start, local_row_end, 0, global_col_n);
  print("global_mat has %lu nonzeros\n", global_mat.nnz());
  print("local_mat has %lu nonzeros\n", local_mat.nnz());

  std::vector<T> local_v(local_row_n, 1);
  DistWrapper<std::vector<T>> dist_v(&local_v);
  std::vector<T> new_v(local_row_n, 0);

  register_amaggrd(get_value, dist_v.local(), index_type());

  SimpleTimer timer;
  barrier();
//  print("SPMV start\n");
  timer.start();
  debug::set_timeout(5);
  for (int k = 0; k < total_steps; ++k) {
    std::unordered_map<index_type, Future<T>> requests_pool;
    for (size_t i = 0; i < local_mat.shape()[0]; i++) {
      for (index_type j_ptr = local_mat.rowptr_[i]; j_ptr < local_mat.rowptr_[i + 1]; j_ptr++) {
        index_type j = local_mat.colind_[j_ptr];

        if (requests_pool.find(j) == requests_pool.end()) {
          rank_t remote_target = j / local_row_n;
          Future<T> fut = rpc_aggrd(remote_target, get_value, dist_v[remote_target], j);
          bool ok = requests_pool.insert({j, std::move(fut)}).second;
          assert(ok);
        }
      }
    }

    flush_agg_buffer(RPC_AGGRD);
    for (size_t i = 0; i < local_mat.shape()[0]; i++) {
      for (index_type j_ptr = local_mat.rowptr_[i]; j_ptr < local_mat.rowptr_[i + 1]; j_ptr++) {
        index_type j = local_mat.colind_[j_ptr];
        T m_value = local_mat.vals_[j_ptr];

        auto it = requests_pool.find(j);
        T v_value = it->second.get();
        new_v[i] += m_value * v_value;
      }
    }
    pure_barrier();
  }
  barrier();
  debug::set_timeout(NO_TIMEOUT);
  timer.end();
  print("%d SPMV steps finished in %.2lf seconds, %.2lf us per SPMV\n", total_steps, timer.to_s(), timer.to_us() / total_steps);
  // profile
  double bw_out = info::networkInfo.byte_send.get() / 1e6;
  double bw_out_s = bw_out / timer.to_s();
  double bw_in = info::networkInfo.byte_recv.get() / 1e6;
  double bw_in_s = bw_in / timer.to_s();
  print("Bandwidth utilization (rank 0): %.2lf MB out (%.2lf MB/s), %.2f MB in (%.2lf MB/s)\n",
        bw_out, bw_out_s, bw_in, bw_in_s);

  if (is_test) {
    auto foutname = string_format("spmv_", rank_me(), ".tmp");
    std::ofstream fout(foutname);
    if (!fout.is_open()) {
      throw std::runtime_error("benchmark_spmv cannot open " + foutname);
    }
    for (size_t i = 0; i < new_v.size(); ++i) {
      size_t global_index = rank_me() * local_row_n + i;
      if (global_index < global_row_n)
        fout << rank_me() * local_row_n + i << "\t" << new_v[i] << "\n";
    }
    fout.close();
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./benchmark_spmv [filename]\n");
    exit(1);
  }

  std::string fname = std::string(argv[1]);

  if (argc > 2) {
    total_steps = atoi(argv[2]);
  }

  if (argc > 3) {
    is_test = (std::string(argv[3]) == "test");
  }

  BCL::CSRMatrix<T, index_type> global_mat(fname);

  arl::init(15, 16);
  arl::run(worker, fname);
  arl::finalize();
  return 0;
}