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
  AM_FF_REQ, AM_FFRD_REQ
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

namespace amaggrd_internal {
extern void generic_amaggrd_reqhandler(const am_internal::UniformGexAMEventData& event);
extern void generic_amaggrd_ackhandler(const am_internal::UniformGexAMEventData& event);
} // namespace amaggrd_internal

namespace amff_internal {
extern void generic_amff_reqhandler(const am_internal::UniformGexAMEventData& event);
} // namespace amff_internal

namespace amffrd_internal {
extern void generic_amffrd_reqhandler(const am_internal::UniformGexAMEventData& event);
} // namespace amffrd_internal


namespace am_internal {
__thread std::queue<UniformGexAMEventData>* am_event_queue_p;

// return value indicates whether this function actually executes some works.
inline bool pool_am_event_queue() {
  if (am_event_queue_p == nullptr) return false;
  bool flag = false;
  while (!am_event_queue_p->empty()) {
    flag = true;
    UniformGexAMEventData event = am_event_queue_p->front();
    am_event_queue_p->pop();
    switch (event.handler_type) {
      case AM_REQ :
        amagg_internal::generic_amagg_reqhandler(event);
        break;
      case AM_ACK :
        amagg_internal::generic_amagg_ackhandler(event);
        break;
      case AM_RD_REQ :
        amaggrd_internal::generic_amaggrd_reqhandler(event);
        break;
      case AM_RD_ACK :
        amaggrd_internal::generic_amaggrd_ackhandler(event);
        break;
      case AM_FF_REQ:
        amff_internal::generic_amff_reqhandler(event);
        break;
      case AM_FFRD_REQ:
        amffrd_internal::generic_amffrd_reqhandler(event);
        break;
    }
    delete [] event.arg_p;
    delete [] event.buf_p;
  }
  return flag;
}

} // namespace am_internal
} // namespace arl

#endif //ARL_AM_QUEUE_HPP
