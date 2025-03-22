#include "arl_internal.hpp"

namespace arl {
namespace amffrd_internal {

// AM synchronous counters
alignas(alignof_cacheline) AlignedAtomicInt64 *amffrd_recv_counter;
alignas(alignof_cacheline) AlignedAtomicInt64 *amffrd_req_counters;// of length proc::rank_n()
// other variables whose names are clear
alignas(alignof_cacheline) AggBuffer **amffrd_agg_buffer_p;
alignas(alignof_cacheline) AmffrdReqMeta *global_meta_p = nullptr;

// Currently, init_am* should only be called once. Multiple call might run out of gex_am_handler_id.
// Should be called after arl::backend::init
void init_amffrd() {
  amffrd_agg_buffer_p = new AggBuffer *[proc::rank_n()];

  int max_buffer_size = backend::get_max_buffer_size();
  for (int i = 0; i < proc::rank_n(); ++i) {
    amffrd_agg_buffer_p[i] = new am_internal::AggBufferAtomic();
    amffrd_agg_buffer_p[i]->init(max_buffer_size, sizeof(AmffrdReqMeta));
  }

  amffrd_recv_counter = new AlignedAtomicInt64;
  amffrd_recv_counter->val = 0;
  amffrd_req_counters = new AlignedAtomicInt64[proc::rank_n()];
  for (int i = 0; i < proc::rank_n(); ++i) {
    amffrd_req_counters[i].val = 0;
  }
}

void exit_amffrd() {
  delete[] amffrd_req_counters;
  delete amffrd_recv_counter;
  delete amffrd_internal::global_meta_p;
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
  ARL_Assert(nbytes >= (int) sizeof(AmffrdReqMeta), "(", nbytes, " >= ", sizeof(AmffrdReqMeta), ")");
  AmffrdReqMeta &meta = *reinterpret_cast<AmffrdReqMeta *>(buf);
  buf += sizeof(AmffrdReqMeta);
  nbytes -= sizeof(AmffrdReqMeta);
  //  printf("recv meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);

  auto invoker = resolve_pi_fnptr<intptr_t(const std::string &)>(meta.type_wrapper_p);
  intptr_t req_invoker_p = invoker("req_invoker");
  auto req_invoker = resolve_pi_fnptr<req_invoker_t>(req_invoker_p);
  int ack_n = req_invoker(meta.fn_p, buf, nbytes);

  amffrd_recv_counter->val += ack_n;
  //  printf("rank %ld exit reqhandler %p, %lu\n", rank_me(), void_buf, unbytes);
}

void sendm_amffrd(rank_t remote_proc, AmffrdReqMeta meta, void *buf, size_t nbytes) {
  memcpy(buf, &meta, sizeof(AmffrdReqMeta));
  //  printf("send meta: %ld, %ld\n", meta.fn_p, meta.type_wrapper_p);
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
          amffrd_internal::sendm_amffrd(i, *amffrd_internal::global_meta_p,
                                        std::get<0>(result), std::get<1>(result));
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
      src_v[i] = amffrd_req_counters[i].val.load();
    }
    auto dst_v = proc::reduce_all(src_v, op_plus());
    expected_recv_num = dst_v[proc::rank_me()];
  }
  return local::broadcast(expected_recv_num, 0);
}

void wait_amffrd() {
  local::barrier();
  int64_t expected_recv_num = get_expected_recv_num();
  progress_external_until([&]() { return expected_recv_num <= amffrd_recv_counter->val; });
}

} // namespace amffrd_internal
} // namespace arl