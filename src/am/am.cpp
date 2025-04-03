#include "arl_internal.hpp"

namespace arl {

namespace am_internal {
AmHandlerRegistry g_amhandler_registry;

void init_am() {
  amagg_internal::init_amagg();
  amff_internal::init_amff();
  amaggrd_internal::init_amaggrd();
  amffrd_internal::init_amffrd();
}

void exit_am() {
  amffrd_internal::exit_amffrd();
  amaggrd_internal::exit_amaggrd();
  amff_internal::exit_amff();
  amagg_internal::exit_amagg();
}
} // namespace am_internal

} // namespace arl