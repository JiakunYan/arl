#include "arl_internal.hpp"

namespace arl::backend::internal {

std::vector<thread_state_t> devices;
std::vector<thread_state_t> thread_states;
bool to_finalize_lci = false;
int64_t num_progress_threads = -1;
int64_t num_workers_per_progress = -1;

int get_rand() {
  // use rand_r for threading efficiency
  static __thread unsigned int seed = -1;
  if (seed == -1) {
    seed = rand();
  }
  return rand_r(&seed);
}

thread_state_t *get_thread_state() {
  int idx = local::rank_me();
  if (idx < 0) {
    idx = 0;
  } else if (idx >= thread_states.size()) {
    // progress thread
    int my_progress_id = idx - local::rank_n();
    int my_worker_start = my_progress_id * num_workers_per_progress;
    int my_worker_end = std::min(my_worker_start + num_workers_per_progress, local::rank_n());
    int range = my_worker_end - my_worker_start;
    idx = my_worker_start + get_rand() % range;
  }
  return &thread_states[idx];
}

void init(size_t custom_num_workers_per_proc,
          size_t custom_num_threads_per_proc) {
  ARL_LOG(INFO, "Initializing LCI backend\n");
  if (!lci::is_active()) {
    lci::g_runtime_init_x().alloc_default_device(false)();
    to_finalize_lci = true;
  } else {
    to_finalize_lci = false;
  }

  num_progress_threads = custom_num_threads_per_proc - custom_num_workers_per_proc;
  if (num_progress_threads > 0) {
    num_workers_per_progress = (custom_num_workers_per_proc + num_progress_threads - 1) / num_progress_threads;
  }

  if (config::lci_ndevices < 0) {
    config::lci_ndevices = custom_num_workers_per_proc;
  }

  size_t npackets = lci::get_default_packet_pool().get_attr_npackets();
  size_t max_nrecvs_per_device = std::min(npackets / 4 / config::lci_ndevices, 4096UL);
  size_t max_nsends_per_device = std::min(npackets / 4 / lci::get_nranks() / config::lci_ndevices, 64UL);
  ARL_LOG(INFO, "Number of devices: %d (max_sends %lu, max_recvs %lu)\n", config::lci_ndevices, max_nsends_per_device, max_nrecvs_per_device);
  devices.resize(config::lci_ndevices);
  lci::comp_t cq;
  lci::rcomp_t rcomp;
  if (config::lci_shared_cq) {
    cq = lci::alloc_cq_x().zero_copy_am(true)();
    rcomp = lci::register_rcomp(cq);
  }
  for (size_t i = 0; i < devices.size(); ++i) {
    auto &device = devices[i];
    device.device = lci::alloc_device_x().net_max_sends(max_nsends_per_device).net_max_recvs(max_nrecvs_per_device)();
    device.endpoint = lci::get_default_endpoint_x().device(device.device)();
    if (config::lci_shared_cq) {
      device.comp = cq;
      device.rcomp = rcomp;
    } else {
      device.comp = lci::alloc_cq_x().zero_copy_am(true)();
      device.rcomp = lci::register_rcomp(device.comp);
    }
  }

  int nthreads_per_device = custom_num_workers_per_proc / config::lci_ndevices;
  thread_states.resize(custom_num_workers_per_proc);
  size_t thread_idx = 0;
  for (size_t i = 0; i < config::lci_ndevices; ++i) {
    for (size_t j = 0; j < nthreads_per_device; ++j) {
      if (thread_idx >= thread_states.size()) {
        break;
      }
      thread_states[thread_idx] = devices[i];
      thread_idx++;
    }
  }
}

void finalize() {
  for (auto &device : devices) {
    lci::free_device(&device.device);
    if (!config::lci_shared_cq) {
      lci::free_comp(&device.comp);
    }
  }
  if (config::lci_shared_cq) {
    lci::free_comp(&devices[0].comp);
  }
  if (lci::is_active() && to_finalize_lci)
    lci::g_runtime_fina();
}

}// namespace arl::backend::internal
