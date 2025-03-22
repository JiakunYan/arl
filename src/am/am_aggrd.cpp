#include "arl_internal.hpp"

namespace arl {
namespace amaggrd_internal {

alignas(alignof_cacheline) AlignedAtomicInt64 *amaggrd_ack_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amaggrd_req_counter;
alignas(alignof_cacheline) AlignedInt64 *amaggrd_req_local_counters;

alignas(alignof_cacheline) AggBuffer **amaggrd_agg_buffer_p;
alignas(alignof_cacheline) AmaggrdReqMeta *global_meta_p = nullptr;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amaggrd() {
  amaggrd_agg_buffer_p = new AggBuffer *[proc::rank_n()];

  // TODO: might have problem if sizeof(result) > sizeof(arguments)
  int max_buffer_size = backend::get_max_buffer_size();
  for (int i = 0; i < proc::rank_n(); ++i) {
    amaggrd_agg_buffer_p[i] = new am_internal::AggBufferAtomic();
    amaggrd_agg_buffer_p[i]->init(max_buffer_size, sizeof(AmaggrdReqMeta));
  }

  amaggrd_ack_counter = new AlignedAtomicInt64;
  amaggrd_ack_counter->val = 0;
  amaggrd_req_counter = new AlignedAtomicInt64;
  amaggrd_req_counter->val = 0;

  amaggrd_req_local_counters = new AlignedInt64[local::rank_n()];
  for (int i = 0; i < local::rank_n(); ++i) {
    amaggrd_req_local_counters[i].val = 0;
  }
}

void exit_amaggrd() {
  delete[] amaggrd_req_local_counters;
  delete global_meta_p;
  delete amaggrd_ack_counter;
  delete amaggrd_req_counter;
  for (int i = 0; i < proc::rank_n(); ++i) {
    delete amaggrd_agg_buffer_p[i];
  }
  delete[] amaggrd_agg_buffer_p;
}

void generic_amaggrd_reqhandler(const backend::cq_entry_t &event) {
  using req_invoker_t = int(intptr_t, char *, int, char *, int);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_RD_REQ);

  char *in_buf = (char *) event.buf;
  int in_nbytes = event.nbytes;
  ARL_Assert(in_nbytes >= (int) sizeof(AmaggrdReqMeta), "(", in_nbytes, " >= ", sizeof(AmaggrdReqMeta), ")");
  AmaggrdReqMeta &in_meta = *reinterpret_cast<AmaggrdReqMeta *>(in_buf);
  in_buf += sizeof(AmaggrdReqMeta);
  in_nbytes -= sizeof(AmaggrdReqMeta);
  //  printf("recv meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
  int out_nbytes = backend::get_max_buffer_size();
  char *out_buf = (char *) backend::buffer_alloc(out_nbytes);
  AmaggrdAckMeta out_meta{in_meta.type_wrapper_p};
  memcpy(out_buf, &out_meta, sizeof(AmaggrdAckMeta));

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(in_meta.type_wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
  int out_consumed = req_invoker(in_meta.fn_p, in_buf, in_nbytes, out_buf + sizeof(AmaggrdAckMeta), out_nbytes - sizeof(AmaggrdAckMeta));
  out_consumed += sizeof(AmaggrdAckMeta);

  backend::send_msg(event.srcRank, am_internal::HandlerType::AM_RD_ACK, out_buf, out_consumed);
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void generic_amaggrd_ackhandler(const backend::cq_entry_t &event) {
  using ack_invoker_t = int(char *, int);
  ARL_Assert(event.tag == am_internal::HandlerType::AM_RD_ACK);

  char *buf = (char *) event.buf;
  int nbytes = event.nbytes;
  ARL_Assert(nbytes >= (int) sizeof(AmaggrdAckMeta), "(", nbytes, " >= ", sizeof(AmaggrdAckMeta), ")");
  AmaggrdAckMeta &meta = *reinterpret_cast<AmaggrdAckMeta *>(buf);
  buf += sizeof(AmaggrdAckMeta);
  nbytes -= sizeof(AmaggrdAckMeta);
  //  printf("recv meta: %ld\n", meta.type_wrapper_p);

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(meta.type_wrapper_p);
  intptr_t ack_invoker_p = invoker("ack_invoker");
  auto ack_invoker = resolve_pi_fnptr<ack_invoker_t>(ack_invoker_p);
  int ack_n = ack_invoker(buf, nbytes);

  amaggrd_ack_counter->val += ack_n;
}

void sendm_amaggrd(rank_t remote_proc, AmaggrdReqMeta meta, void *buf, size_t nbytes) {
  memcpy(buf, &meta, sizeof(AmaggrdReqMeta));
  //  printf("send meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
  ARL_Assert(nbytes >= sizeof(AmaggrdReqMeta));
  backend::send_msg(remote_proc, am_internal::HandlerType::AM_RD_REQ, buf, nbytes);
}

void flush_amaggrd_buffer() {
  for (int ii = 0; ii < proc::rank_n(); ++ii) {
    int i = (ii + local::rank_me()) % proc::rank_n();
    std::vector<std::pair<char *, int64_t>> results = amaggrd_agg_buffer_p[i]->flush();
    for (auto result : results) {
      if (std::get<0>(result) != nullptr) {
        if (std::get<1>(result) != 0) {
          //          printf("rank %ld send %p, %d\n", rank_me(), std::get<0>(result), std::get<1>(result));
          sendm_amaggrd(i, *amaggrd_internal::global_meta_p, std::get<0>(result), std::get<1>(result));
          progress_external();
        }
      }
    }
  }
}

int64_t get_amaggrd_buffer_size() {
  int64_t value = 0;
  for (int i = 0; i < proc::rank_n(); ++i) {
    value += amaggrd_agg_buffer_p[i]->get_size();
  }
  return value;
}

std::string get_amaggrd_buffer_status() {
  std::ostringstream os;
  for (int i = 0; i < proc::rank_n(); ++i) {
    std::string status = amaggrd_agg_buffer_p[i]->get_status();
    os << "No. " << i << "\n";
    os << status;
  }
  return os.str();
}

void wait_amaggrd() {
  amaggrd_req_counter->val += amaggrd_req_local_counters[local::rank_me()].val;
  amaggrd_req_local_counters[local::rank_me()].val = 0;
  local::barrier();
  progress_external_until([&]() { return amaggrd_req_counter->val <= amaggrd_ack_counter->val; });
}

} // namespace amaggrd_internal
} // namespace arl