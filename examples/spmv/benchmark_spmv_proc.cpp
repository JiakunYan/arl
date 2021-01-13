//
// Created by jiakun on 2020/8/28.
//
#define ARL_INFO
#include "arl.hpp"
#include "csrmat/CSRMatrix.hpp"

using namespace arl;

using T = double;
using index_type = int64_t;

bool is_test = false;
int total_steps = 100;

std::vector<T> *proc_v;

T get_value(index_type global_index) {
  index_type proc_v_n = proc_v->size();
  ARL_Assert(global_index >= proc_v_n * proc::rank_me());
  ARL_Assert(global_index < proc_v_n * (proc::rank_me() + 1));
  index_type local_index = global_index % proc_v_n;
  T val = proc_v->at(local_index);
  return val;
}

T get_value_local(index_type global_index) {
  index_type proc_v_n = proc_v->size();
  ARL_Assert(global_index >= proc_v_n * proc::rank_me());
  ARL_Assert(global_index < proc_v_n * (proc::rank_me() + 1));
  index_type local_index = global_index % proc_v_n;
  T val = proc_v->at(local_index);
  return val;
}

void proc_setup(const BCL::CSRMatrix<T, index_type>& global_mat) {
  index_type global_row_n = global_mat.shape()[0];
  index_type proc_row_n = ceil((double) global_row_n / proc::rank_n());
  proc_v = new std::vector<T>(proc_row_n, 1);
}


void worker(const BCL::CSRMatrix<T, index_type>& global_mat) {
  index_type global_row_n = global_mat.shape()[0];
  index_type local_row_n = ceil((double) global_row_n / rank_n());
  index_type local_row_start = std::min(local_row_n * (index_type)rank_me(), global_row_n);
  index_type local_row_end = std::min(local_row_n * ((index_type)rank_me() + 1), global_row_n);
  index_type global_col_n = global_mat.shape()[1];
  assert(global_row_n == global_col_n);
  index_type proc_col_n = ceil((double) global_row_n / proc::rank_n());
  index_type proc_col_start = std::min(proc_col_n * (index_type)proc::rank_me(), global_col_n);;
  index_type proc_col_end = std::min(proc_col_n * ((index_type)proc::rank_me() + 1), global_col_n);

  BCL::CSRMatrix<T, index_type> local_mat =
      global_mat.get_slice_impl_(local_row_start, local_row_end, proc_col_start, proc_col_end);

  BCL::CSRMatrix<T, index_type> remote_mat1 =
      global_mat.get_slice_impl_(local_row_start, local_row_end, 0, proc_col_start);
  BCL::CSRMatrix<T, index_type> remote_mat2 =
      global_mat.get_slice_impl_(local_row_start, local_row_end, proc_col_end, global_col_n);
  print("global_mat has %lu nonzeros\n", global_mat.nnz());
  print("local_mat has %lu nonzeros\n", local_mat.nnz());
  print("remote_mat1 has %lu nonzeros\n", remote_mat1.nnz());
  print("remote_mat2 has %lu nonzeros\n", remote_mat2.nnz());

  std::vector<T> new_v(local_row_n, 0);

  register_amaggrd(get_value, index_type());

  debug::set_timeout(5);
  SimpleTimer timer;
  SimpleTimer timer_rpc;
  SimpleTimer timer_local_comp;
  barrier();
  timer.start();
  for (int k = 0; k < total_steps; ++k) {
    std::unordered_map<index_type, Future<T>> requests_pool;
    // remote_mat1
    for (index_type i = 0; i < remote_mat1.shape()[0]; i++) {
      for (index_type j_ptr = remote_mat1.rowptr_[i]; j_ptr < remote_mat1.rowptr_[i + 1]; j_ptr++) {
        index_type j = remote_mat1.colind_[j_ptr];
        index_type global_j = j;

        if (requests_pool.find(global_j) == requests_pool.end()) {
          rank_t remote_target = global_j / proc_col_n;
          timer_rpc.start();
          Future<T> fut = rpc_aggrd(remote_target, get_value, global_j);
          timer_rpc.end();
          bool ok = requests_pool.insert({global_j, std::move(fut)}).second;
          assert(ok);
        }
      }
    }

    // remote_mat2
    for (index_type i = 0; i < remote_mat2.shape()[0]; i++) {
      for (index_type j_ptr = remote_mat2.rowptr_[i]; j_ptr < remote_mat2.rowptr_[i + 1]; j_ptr++) {
        index_type j = remote_mat2.colind_[j_ptr];
        index_type global_j = j + proc_col_end;

        if (requests_pool.find(global_j) == requests_pool.end()) {
          rank_t remote_target = global_j / proc_col_n;
          timer_rpc.start();
          Future<T> fut = rpc_aggrd(remote_target, get_value, global_j);
          timer_rpc.end();
          bool ok = requests_pool.insert({global_j, std::move(fut)}).second;
          assert(ok);
        }
      }
    }
    flush_agg_buffer(RPC_AGGRD);

    // compute local_mat
    timer_local_comp.start();
    for (index_type i = 0; i < local_mat.shape()[0]; i++) {
      for (index_type j_ptr = local_mat.rowptr_[i]; j_ptr < local_mat.rowptr_[i + 1]; j_ptr++) {
        index_type j = local_mat.colind_[j_ptr];
        index_type global_j = j + proc_col_start;
        T m_value = local_mat.vals_[j_ptr];
        T v_value = get_value_local(global_j);
        new_v[i] += m_value * v_value;
      }
    }
    timer_local_comp.end();

    // compute remote_mat1
    for (index_type i = 0; i < remote_mat1.shape()[0]; i++) {
      for (index_type j_ptr = remote_mat1.rowptr_[i]; j_ptr < remote_mat1.rowptr_[i + 1]; j_ptr++) {
        index_type j = remote_mat1.colind_[j_ptr];
        index_type global_j = j;
        T m_value = remote_mat1.vals_[j_ptr];

        auto it = requests_pool.find(global_j);
        T v_value = it->second.get();
        new_v[i] += m_value * v_value;
      }
    }
    // compute remote_mat2
    for (index_type i = 0; i < remote_mat2.shape()[0]; i++) {
      for (index_type j_ptr = remote_mat2.rowptr_[i]; j_ptr < remote_mat2.rowptr_[i + 1]; j_ptr++) {
        index_type j = remote_mat2.colind_[j_ptr];
        index_type global_j = j + proc_col_end;
        T m_value = remote_mat2.vals_[j_ptr];

        auto it = requests_pool.find(global_j);
        T v_value = it->second.get();
        new_v[i] += m_value * v_value;
      }
    }
    pure_barrier();
  }
  barrier();
  timer.end();
  debug::set_timeout(NO_TIMEOUT);
  print("%d SPMV steps finished in %.2lf seconds, %.2lf us per SPMV\n", total_steps, timer.to_s(), timer.to_us() / total_steps);
  // profile
  double bw_out_local = info::networkInfo.byte_send.get() / 1e6;
  double bw_in_local = info::networkInfo.byte_recv.get() / 1e6;
  double bw_out = reduce_all(bw_out_local, op_plus());
  double bw_in = reduce_all(bw_in_local, op_plus());
  double bw_out_s = bw_out / timer.to_s();
  double bw_in_s = bw_in / timer.to_s();
  print("Bandwidth utilization: %.2lf MB out (%.2lf MB/s), %.2f MB in (%.2lf MB/s)\n",
      bw_out, bw_out_s, bw_in, bw_in_s);
  timer_rpc.col_print_us("rpc");
  timer_local_comp.col_print_s("local computation");

  if (is_test) {
    auto foutname = string_format("spmv_", rank_me(), ".tmp");
    std::ofstream fout(foutname);
    if (!fout.is_open()) {
      throw std::runtime_error("benchmark_spmv cannot open " + foutname);
    }
    for (index_type i = 0; i < new_v.size(); ++i) {
      index_type global_index = rank_me() * local_row_n + i;
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
  proc_setup(global_mat);
  arl::run(worker, global_mat);
  delete proc_v;
  arl::finalize();
  return 0;
}