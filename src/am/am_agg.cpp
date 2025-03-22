#include "arl_internal.hpp"

namespace arl {

namespace amagg_internal {
alignas(alignof_cacheline) AlignedAtomicInt64 *amagg_ack_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amagg_req_counter;
alignas(alignof_cacheline) AlignedInt64 *amagg_req_local_counters;
alignas(alignof_cacheline) AggBuffer **amagg_agg_buffer_p;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amagg() {
  amagg_agg_buffer_p = new AggBuffer *[proc::rank_n()];

  // TODO: might have problem if sizeof(result) > sizeof(arguments)
  int max_buffer_size = backend::get_max_buffer_size();
  for (int i = 0; i < proc::rank_n(); ++i) {
    if (config::aggBufferType == config::AGG_BUFFER_SIMPLE)
      amagg_agg_buffer_p[i] = new am_internal::AggBufferSimple();
    else if (config::aggBufferType == config::AGG_BUFFER_LOCAL)
      amagg_agg_buffer_p[i] = new am_internal::AggBufferLocal();
    else {
      assert(config::aggBufferType == config::AGG_BUFFER_ATOMIC);
      amagg_agg_buffer_p[i] = new am_internal::AggBufferAtomic();
    }
    amagg_agg_buffer_p[i]->init(max_buffer_size, 0);
  }
  amagg_ack_counter = new AlignedAtomicInt64;
  amagg_ack_counter->val = 0;
  amagg_req_counter = new AlignedAtomicInt64;
  amagg_req_counter->val = 0;

  amagg_req_local_counters = new AlignedInt64[local::rank_n()];
  for (int i = 0; i < local::rank_n(); ++i) {
    amagg_req_local_counters[i].val = 0;
  }
}

void exit_amagg() {
  delete[] amagg_req_local_counters;
  delete amagg_req_counter;
  delete amagg_ack_counter;
  for (int i = 0; i < proc::rank_n(); ++i) {
    delete amagg_agg_buffer_p[i];
  }
  delete[] amagg_agg_buffer_p;
}

void generic_amagg_reqhandler(const backend::cq_entry_t &event) {
  using req_invoker_t = std::pair<int, int>(intptr_t, int, char *, int, char *, int);
  //  printf("rank %ld amagg reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_REQ);

  char *buf = (char *) event.buf;
  int nbytes = event.nbytes;
  int o_cap = backend::get_max_buffer_size();
  char *o_buf = (char *) backend::buffer_alloc(o_cap);
  int o_consumed = 0;
  int n = 0;
  int consumed = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmaggReqMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmaggReqMeta), ")");
    AmaggReqMeta &meta = *reinterpret_cast<AmaggReqMeta *>(buf + consumed);
    //    printf("meta: %ld, %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.future_p, meta.target_local_rank);
    consumed += sizeof(AmaggReqMeta);

    ARL_Assert(o_cap >= o_consumed + (int) sizeof(AmaggAckMeta), "(", o_cap, " >= ", o_consumed, " + ", sizeof(AmaggReqMeta), ")");
    AmaggAckMeta &ackMeta = *reinterpret_cast<AmaggAckMeta *>(o_buf + o_consumed);
    ackMeta.type_wrapper_p = meta.type_wrapper_p;
    ackMeta.future_p = meta.future_p;
    o_consumed += sizeof(AmaggAckMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(meta.type_wrapper_p);
    intptr_t req_invoker_p = invoker("req_invoker");
    auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
    //    printf("o_consumed is %d\n", o_consumed);
    std::pair<int, int> consumed_data = req_invoker(meta.fn_p, meta.target_local_rank, buf + consumed, nbytes - consumed, o_buf + o_consumed, o_cap - o_consumed);
    consumed += std::get<0>(consumed_data);
    o_consumed += std::get<1>(consumed_data);
    ++n;
  }
  backend::send_msg(event.srcRank, am_internal::HandlerType::AM_ACK, o_buf, o_consumed);
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void generic_amagg_ackhandler(const backend::cq_entry_t &event) {
  using ack_invoker_t = int(intptr_t, char *, int);
  //  printf("rank %ld amagg ackhandler %p, %lu\n", rank_me(), void_buf, unbytes);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_ACK);

  char *buf = (char *) event.buf;
  int nbytes = event.nbytes;
  int consumed = 0;
  int n = 0;
  while (nbytes > consumed) {
    ARL_Assert(nbytes >= consumed + (int) sizeof(AmaggAckMeta), "(", nbytes, " >= ", consumed, " + ", sizeof(AmaggAckMeta), ")");
    AmaggAckMeta &meta = *reinterpret_cast<AmaggAckMeta *>(buf + consumed);
    //    printf("meta: %ld, %ld, %d\n", meta.fn_p, meta.type_wrapper_p, meta.target_local_rank);
    consumed += sizeof(AmaggAckMeta);

    auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(meta.type_wrapper_p);
    intptr_t ack_invoker_p = invoker("ack_invoker");
    auto ack_invoker = resolve_pi_fnptr<ack_invoker_t>(ack_invoker_p);
    consumed += ack_invoker(meta.future_p, buf + consumed, nbytes - consumed);
    ++n;
  }
  amagg_internal::amagg_ack_counter->val += n;
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void flush_amagg_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char *, int64_t>> results = amagg_agg_buffer_p[i]->flush();
    for (auto result : results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          //          printf("rank %ld send %p, %d\n", rank_me(), std::get<0>(result), std::get<1>(result));
          backend::send_msg(i, am_internal::HandlerType::AM_REQ, std::get<0>(result), std::get<1>(result));
          progress_external();
        }
      }
    }
  }
}

void wait_amagg() {
  amagg_req_counter->val += amagg_req_local_counters[local::rank_me()].val;
  amagg_req_local_counters[local::rank_me()].val = 0;
  local::barrier();
  progress_external_until([&]() { return amagg_req_counter->val <= amagg_ack_counter->val; });
}

} // namespace amagg_internal
} // namespace arl