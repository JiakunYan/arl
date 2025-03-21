//
// Created by Jiakun Yan on 5/13/20.
//

#ifndef ARL_AM_PROGRESS_HPP
#define ARL_AM_PROGRESS_HPP

namespace arl::am_internal {
enum HandlerType {
  AM_REQ,
  AM_ACK,
  AM_RD_REQ,
  AM_RD_ACK,
  AM_FF_REQ,
  AM_FFRD_REQ
};

// return value indicates whether this function actually executes some works.
inline bool pool_am_event_queue() {
  backend::cq_entry_t event;
  int ret = backend::poll_msg(event);
  if (ret == ARL_RETRY) return false;
  switch (event.tag) {
    case AM_REQ:
      amagg_internal::generic_amagg_reqhandler(event);
      break;
    case AM_ACK:
      amagg_internal::generic_amagg_ackhandler(event);
      break;
    case AM_RD_REQ:
      amaggrd_internal::generic_amaggrd_reqhandler(event);
      break;
    case AM_RD_ACK:
      amaggrd_internal::generic_amaggrd_ackhandler(event);
      break;
    case AM_FF_REQ:
      amff_internal::generic_amff_reqhandler(event);
      break;
    case AM_FFRD_REQ:
      amffrd_internal::generic_amffrd_reqhandler(event);
      break;
    default:
      fprintf(stderr, "Unknown tag %d\n", event.tag);
      exit(0);
  }
  backend::buffer_free(event.buf);
  return true;
}

}// namespace arl::am_internal

#endif//ARL_AM_PROGRESS_HPP
