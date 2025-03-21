//
// Created by Jiakun Yan on 10/21/19.
//

#ifndef ARL_UTILS_HPP
#define ARL_UTILS_HPP

// output stream
namespace arl {
template<typename T>
constexpr int my_sizeof() {
  if constexpr (std::is_void_v<T>) {
    return 0;
  } else {
    return sizeof(T);
  }
}

inline void find_and_replace(std::string &subject, const std::string &search, const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

inline void os_format(std::ostringstream &os) {}

template<typename T, typename... Params>
inline void os_format(std::ostringstream &os, T first, Params... params) {
  os << first;
  os_format(os, params...);
}

template<typename... Params>
inline std::string string_format(Params... params) {
  std::ostringstream os;
  os_format(os, params...);
  return os.str();
}

template<typename T, typename... Params>
inline void os_print(std::ostream &stream, T first, Params... params) {
  std::ostringstream os;
  os << first;
  os_format(os, params...);
  if (stream.rdbuf() != std::cout.rdbuf() && stream.rdbuf() != std::cerr.rdbuf()) {
    // strip out colors for log file
    std::string outstr = os.str();
    for (auto c : COLORS) find_and_replace(outstr, c, "");
    stream << outstr;
  } else {
    stream << os.str();
  }
  stream.flush();
}

#ifdef ARL_DEBUG
#define ARL_Assert(Expr, ...) \
  __ARL_Assert(#Expr, Expr, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define ARL_Assert(Expr, ...) ;
#endif

template<typename... Params>
void __ARL_Assert(const char *expr_str, bool expr, const char *file, int line, Params... params) {
  std::ostringstream os;
  os_format(os, params...);
  if (!expr) {
    std::cerr << "Assert failed:\t" << os.str() << "\n"
              << "Expected:\t" << expr_str << "\n"
              << "Source:\t\t" << file << ", line " << line << "\n";
    abort();
  }
}

#define ARL_Error(...) \
  __ARL_Error(__FILE__, __LINE__, ##__VA_ARGS__)

template<typename... Params>
void __ARL_Error(const char *file, int line, Params... params) {
  std::ostringstream os;
  os_format(os, params...);
  std::cerr << "ERROR:\t" << os.str() << "\n"
            << "Source:\t\t" << file << ", line " << line << std::endl;
  abort();
}

#define ARL_Warn(...) \
  __ARL_Warn(__FILE__, __LINE__, ##__VA_ARGS__)

template<typename... Params>
void __ARL_Warn(const char *file, int line, Params... params) {
  std::ostringstream os;
  os_format(os, params...);
  std::cerr << "WARNING:\t" << os.str() << "\n"
            << "Source:\t\t" << file << ", line " << line << std::endl;
}


#define ARL_Assert_Align(Val, alignof_size) ARL_Assert(alignof(Val) % alignof_size == 0, "alignment check failed!")
}// namespace arl

#endif//ARL_UTILS_HPP
