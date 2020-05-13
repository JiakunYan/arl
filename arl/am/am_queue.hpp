//
// Created by Jiakun Yan on 5/13/20.
//

#ifndef ARL_AM_QUEUE_HPP
#define ARL_AM_QUEUE_HPP

namespace arl {
namespace am_internal {
enum HandlerType {
  AM_REQ, AM_ACK,
  AM_RD_REQ, AM_RD_ACK,
  AM_FF_REQ, AM_FF_RD_REQ
};

struct UniformGexAMEventData {
  HandlerType handler_type;
  gex_Rank_t srcRank;
  int arg_n;
  gex_AM_Arg_t *arg_p;
  int buf_n;
  char *buf_p;
};
} // namespace am_internal


namespace amagg_internal {
extern void generic_amagg_reqhandler(const am_internal::UniformGexAMEventData& event);
extern void generic_amagg_ackhandler(const am_internal::UniformGexAMEventData& event);
} // namespace amagg_internal

namespace am_internal {
__thread std::queue<UniformGexAMEventData>* am_event_queue_p;

inline void poll_am_event_queue() {
  if (am_event_queue_p == nullptr) return;
  while (!am_event_queue_p->empty()) {
    UniformGexAMEventData event = am_event_queue_p->front();
    am_event_queue_p->pop();
    switch (event.handler_type) {
      case AM_REQ :
        amagg_internal::generic_amagg_reqhandler(event);
        break;
      case AM_ACK :
        amagg_internal::generic_amagg_ackhandler(event);
        break;
    }
    delete [] event.arg_p;
    delete [] event.buf_p;
  }
}

} // namespace am_internal
} // namespace arl

#endif //ARL_AM_QUEUE_HPP
