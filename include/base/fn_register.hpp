#ifndef ARL_FN_REGISTER_HPP
#define ARL_FN_REGISTER_HPP

#include <functional>
#include <unordered_map>
#include <vector>

namespace arl {

using FnHandler = int;

namespace internal {

struct HandlerRegisterEntry {
  uintptr_t fnptr;
  uintptr_t invoker;
};

extern std::vector<HandlerRegisterEntry> g_handler_registry;

template<typename Fn>
FnHandler register_fn(Fn &&fn) {
  HandlerRegisterEntry entry;
  entry.fnptr = &fn;
  entry.invoker = &AmaggTypeWrapper<std::remove_reference_t<Fn>, std::remove_reference_t<Args>...>::invoker;
  g_handler_registry.push_back(entry);
  return g_handler_registry.size() - 1;
}

} // namespace internal
} // namespace arl

#define ARL_REGISTER_FN(name, fn) ::arl::FnHandler ARL_FNHANDLER_##name = arl::internal::register_fn(fn)

#endif//ARL_FN_REGISTER_HPP