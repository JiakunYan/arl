#include "arl_internal.hpp"

namespace arl::backend::internal {

rank_t my_proc;
rank_t nprocs;
bool finalized;

gex_Client_t client;
gex_EP_t ep;
gex_TM_t tm;

gasnet_seginfo_t *gasnet_seginfo;

std::queue<cq_entry_t> *cq_p;
std::mutex cq_mutex;

void gex_reqhandler(gex_Token_t token, void *void_buf, size_t unbytes, gex_AM_Arg_t tag) {
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t srcRank = info.gex_srcrank;
  char *buf_p = new char[unbytes];
  memcpy(buf_p, void_buf, unbytes);
  cq_entry_t event_p = {
          .srcRank = srcRank,
          .tag = static_cast<tag_t>(tag),
          .buf = buf_p,
          .nbytes = static_cast<int>(unbytes)};
  cq_mutex.lock();
  cq_p->push(event_p);
  cq_mutex.unlock();
}

} // namespace arl::backend::internal