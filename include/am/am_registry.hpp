#ifndef ARL_AM_REGISTRY_HPP
#define ARL_AM_REGISTRY_HPP

namespace arl::am_internal {

// --- Function Traits ---
template<typename T>
struct function_traits;

template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> {
    static constexpr std::size_t arity = sizeof...(Args);
    static constexpr std::size_t total_arg_size = (sizeof(Args) + ... + 0);
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template<typename T>
struct function_traits : function_traits<decltype(&T::operator())> {};

template<typename ClassType, typename Ret, typename... Args>
struct function_traits<Ret(ClassType::*)(Args...) const> {
    static constexpr std::size_t arity = sizeof...(Args);
    static constexpr std::size_t total_arg_size = (sizeof(Args) + ... + 0);
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

// --- Registry ---

class AmHandlerRegistry {
public:
    template<typename... Args>
    static constexpr size_t sizeof_args() {
        return (sizeof(Args) + ... + 0);
    }

    template<typename... Args>
    static void serialize_args(char*& buffer, const Args&... args) {
        (serialize_one(buffer, args), ...);
    }

    template<typename T>
    static void serialize_one(char*& buffer, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable types supported.");
        std::memcpy(buffer, &value, sizeof(T));
        buffer += sizeof(T);
    }

    template<typename T>
    static T deserialize_one(const char*& buffer) {
        T value;
        std::memcpy(&value, buffer, sizeof(T));
        buffer += sizeof(T);
        return value;
    }

    template<typename... Args>
    static std::tuple<Args...> deserialize_args(const void* raw_buffer) {
        const char* buffer = static_cast<const char*>(raw_buffer);
        return std::tuple<Args...>{ deserialize_one<Args>(buffer)... };
    }

    template<typename Fn>
    uint8_t register_amhandler(Fn&& fn) {
      using traits = function_traits<std::decay_t<Fn>>;
      using Tuple = typename traits::args_tuple;
      using Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>;

      uint8_t id = handlers_.size();
      handlers_.emplace_back(make_invoker(std::forward<Fn>(fn), Tuple{}, Indices{}));
      return id;
    }

    void invoke(int id, const void* buffer) {
        if (id >= handlers_.size()) {
            std::cerr << "Invalid function ID: " << id << "\n";
            return;
        }
        auto handler = handlers_[id];
        if (handler == nullptr) {
            std::cerr << "No function registered with ID " << id << "\n";
            return;
        }
        // Call the function with the provided buffer
        handler(buffer);
    }

private:
    // Deserialization and invocation
    template<typename Fn, typename Tuple, std::size_t... I>
    static auto make_invoker(Fn&& fn, Tuple, std::index_sequence<I...>) {
        return [fn = std::forward<Fn>(fn)](const void* buffer) {
            const char* ptr = static_cast<const char*>(buffer);
            auto args = std::make_tuple(deserialize_one<std::tuple_element_t<I, Tuple>>(ptr)...);
            std::apply(fn, args);
        };
    }

    std::vector<std::function<void(const void*)>> handlers_;
};

extern AmHandlerRegistry g_amhandler_registry;

template<typename Fn>
uint8_t register_amhandler(Fn&& fn) {
  return g_amhandler_registry.register_amhandler(fn);
}

} // namespace arl::am_internal

#endif // ARL_AM_REGISTRY_HPP