#include "arl_internal.hpp"

namespace arl {
namespace amff_internal {

// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_recv_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amff_req_counters;// of length proc::rank_n()

alignas(alignof_cacheline) AggBuffer **amff_agg_buffer_p;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amff() {
  amff_agg_buffer_p = new AggBuffer *[proc::rank_n()];

  int max_buffer_size = std::min(config::max_buffer_size, backend::get_max_buffer_size());
  for (int i = 0; i < proc::rank_n(); ++i) {
    if (config::aggBufferType == config::AGG_BUFFER_SIMPLE)
      amff_agg_buffer_p[i] = new am_internal::AggBufferSimple();
    else if (config::aggBufferType == config::AGG_BUFFER_LOCAL)
      amff_agg_buffer_p[i] = new am_internal::AggBufferLocal();
    else {
      assert(config::aggBufferType == config::AGG_BUFFER_ATOMIC);
      amff_agg_buffer_p[i] = new am_internal::AggBufferAtomic();
    }
    amff_agg_buffer_p[i]->init(max_buffer_size, 0);
  }
  amff_recv_counter = new AlignedAtomicInt64;
  amff_recv_counter->val = 0;
  amff_req_counters = new AlignedAtomicInt64[proc::rank_n()];
  for (int i = 0; i < proc::rank_n(); ++i) {
    amff_req_counters[i].val = 0;
  }
}

void exit_amff() {
  delete[] amff_req_counters;
  delete amff_recv_counter;
  for (int i = 0; i < proc::rank_n(); ++i) {
    delete amff_agg_buffer_p[i];
  }
  delete[] amff_agg_buffer_p;
}

void generic_amff_reqhandler(const backend::cq_entry_t &event) {
  using payload_size_t = int();
  using req_invoker_t = void(intptr_t, int, char *, int);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_FF_REQ);
  //  printf("rank %ld amff reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);

  char *buf = (char *) event.buf;
  int nbytes = event.nbytes;
  int n = 0;
  int consumed = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmffReqMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmffReqMeta), ")");
    AmffReqMeta &meta = *reinterpret_cast<AmffReqMeta *>(buf + consumed);
    //    printf("meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
    consumed += sizeof(AmffReqMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(meta.type_wrapper_p);

    intptr_t payload_size_p = invoker("payload_size");
    auto payload_size = resolve_pi_fnptr<payload_size_t>(payload_size_p);
    int payload_nbytes = payload_size();
    //    printf("payload_nbytes: %d\n", payload_nbytes);

    intptr_t req_invoker_p = invoker("req_invoker");
    auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
    req_invoker(meta.fn_p, meta.target_local_rank, buf + consumed, nbytes - consumed);
    consumed += payload_nbytes;
    ++n;
  }

  //  printf("rank %ld increase amff recv counter %ld by %d\n", rank_me(), amff_recv_counter->val.load(), n);
  amff_recv_counter->val += n;
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void flush_amff_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char *, int64_t>> results = amff_agg_buffer_p[i]->flush();
    for (auto result : results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          backend::send_msg(i, am_internal::HandlerType::AM_FF_REQ, std::get<0>(result), std::get<1>(result));
          progress_external();
        }
      }
    }
  }
}

int64_t get_expected_recv_num() {
  int64_t expected_recv_num = 0;
  if (local::rank_me() == 0) {
    std::vector<int64_t> src_v(proc::rank_n());
    for (int i = 0; i < proc::rank_n(); ++i) {
      src_v[i] = amff_req_counters[i].val.load();
    }
    auto dst_v = proc::reduce_all(src_v, op_plus());
    expected_recv_num = dst_v[proc::rank_me()];
  }
  return local::broadcast(expected_recv_num, 0);
}

void wait_amff() {
  local::barrier();
  int64_t expected_recv_num = get_expected_recv_num();
  progress_external_until([&]() { return expected_recv_num <= amff_recv_counter->val; });
}

} // namespace amff_internal
} // namespace arl