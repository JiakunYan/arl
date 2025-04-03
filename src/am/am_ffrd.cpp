#include "arl_internal.hpp"

namespace arl {
namespace amffrd_internal {

// AM synchronous counters
AlignedAtomicInt64 *amffrd_recv_counter;
std::vector<std::vector<int64_t>> *amffrd_req_counters;// of length (local::rank_n, proc::rank_n)
// other variables whose names are clear
AggBuffer **amffrd_agg_buffer_p;
uint8_t g_am_idx = -1;
size_t g_payload_size = 0;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amffrd() {
  amffrd_agg_buffer_p = new AggBuffer *[proc::rank_n()];

  int max_buffer_size = std::min(config::max_buffer_size, backend::get_max_buffer_size());
  for (int i = 0; i < proc::rank_n(); ++i) {
    if (config::aggBufferType == config::AGG_BUFFER_SIMPLE)
      amffrd_agg_buffer_p[i] = new am_internal::AggBufferSimple();
    else if (config::aggBufferType == config::AGG_BUFFER_LOCAL)
      amffrd_agg_buffer_p[i] = new am_internal::AggBufferLocal();
    else {
      assert(config::aggBufferType == config::AGG_BUFFER_ATOMIC);
      amffrd_agg_buffer_p[i] = new am_internal::AggBufferAtomic();
    }
    amffrd_agg_buffer_p[i]->init(max_buffer_size, 0);
  }

  amffrd_recv_counter = new AlignedAtomicInt64;
  amffrd_recv_counter->val = 0;
  amffrd_req_counters = new std::vector<std::vector<int64_t>>();
  amffrd_req_counters->resize(local::rank_n());
  for (auto &v : *amffrd_req_counters) {
    v.resize(proc::rank_n(), 0);
  }
}

void exit_amffrd() {
  delete amffrd_req_counters;
  delete amffrd_recv_counter;
  for (int i = 0; i < proc::rank_n(); ++i) {
    delete amffrd_agg_buffer_p[i];
  }
  delete[] amffrd_agg_buffer_p;
}

void generic_amffrd_reqhandler(const backend::cq_entry_t &event) {
  using req_invoker_t = int(intptr_t, char *, int);
  //  printf("rank %ld reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_FFRD_REQ);

  char *buf = (char *) event.buf;
  int nbytes = event.nbytes;
  ARL_Assert(event.nbytes % g_payload_size == 0, "(", event.nbytes, " % ", g_payload_size, ")");
  size_t n = nbytes / g_payload_size;

  for (size_t i = 0; i < n; ++i) {
    const void *buf = ((char *) event.buf) + i * g_payload_size;

    // rank_t mContext = rank_internal::get_context();
    // rank_internal::set_context(context);
    am_internal::g_amhandler_registry.invoke(g_am_idx, buf);
    // rank_internal::set_context(mContext);
  }

  amffrd_recv_counter->val += n;
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void sendm_amffrd(rank_t remote_proc, void *buf, size_t nbytes) {
  ARL_Assert(nbytes >= sizeof(AmffrdReqMeta));
  backend::send_msg(remote_proc, am_internal::HandlerType::AM_FFRD_REQ, buf, nbytes);
}

void flush_amffrd_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char *, int64_t>> results = amffrd_internal::amffrd_agg_buffer_p[i]->flush();
    for (auto result : results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          amffrd_internal::sendm_amffrd(i, std::get<0>(result), std::get<1>(result));
          progress_external();
        }
      }
    }
  }
}

int64_t get_expected_recv_num() {
  auto dst_v = reduce_all((*amffrd_req_counters)[local::rank_me()], op_plus());
  return dst_v[proc::rank_me()];
}

void wait_amffrd() {
  local::barrier();
  int64_t expected_recv_num = get_expected_recv_num();
  progress_external_until([&]() { return expected_recv_num <= amffrd_recv_counter->val; });
}

} // namespace amffrd_internal
} // namespace arl