#ifndef __UTILS_H
#define __UTILS_H

#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <limits>
#include <regex>
#include <upcxx/upcxx.hpp>

#include "bytell_hash_map.hpp"
#include "colors.h"

using std::string;
using std::stringstream;
using std::ostringstream;
using std::ostream;
using std::ifstream;
using std::ofstream;
using std::to_string;
using std::cout;
using std::cerr;
using std::min;

#ifndef NDEBUG
#define DEBUG
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define ONE_MB (1024*1024)
#define ONE_GB (ONE_MB*1024)

#ifdef USE_BYTELL
#define HASH_TABLE ska::bytell_hash_map
#else
#define HASH_TABLE std::unordered_map
#endif


extern ofstream _logstream;
extern bool _verbose;

inline void find_and_replace(std::string& subject, const std::string& search, const std::string& replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

inline void init_logger() {
  if (!upcxx::rank_me()) _logstream.open("kcount.log");
}

inline void set_logger_verbose(bool verbose) {
  _verbose = verbose;
}

inline void logger(ostringstream &os) {}

template <typename T, typename... Params>
inline void logger(ostringstream &os, T first, Params... params) {
  os << first;
  logger(os, params ...);
}

template <typename T, typename... Params>
inline void logger(ostream &stream, bool fail, bool serial, bool flush, T first, Params... params) {
  if (serial && upcxx::rank_me()) return;
  ostringstream os;
  os << first;
  logger(os, params ...);
  if (fail) {
    std::cerr << "\n" << KNORM;
    throw std::runtime_error(os.str());
  }
  if (stream.rdbuf() != std::cout.rdbuf() && stream.rdbuf() != std::cerr.rdbuf()) {
    // strip out colors for log file
    string outstr = os.str();
    for (auto c : COLORS) find_and_replace(outstr, c, "");
    stream << outstr;
  } else {
    stream << os.str();
  }
  if (flush) stream.flush();
}


#define SOUT(...) do {                                   \
    logger(cout, false, true, true, ##__VA_ARGS__);      \
  } while (0)
#define WARN(...)                                                       \
  logger(cerr, false, false, true, KRED, "\n[", upcxx::rank_me(), "] <", __FILENAME__, ":", __LINE__, \
         "> WARNING: ", ##__VA_ARGS__, KNORM, "\n")
#define DIE(...)                                                        \
  logger(cerr, true, false, true, KLRED, "\n[", upcxx::rank_me(), "] <", __FILENAME__ , ":", __LINE__, \
         "> ERROR: ", ##__VA_ARGS__, KNORM, "\n")
#define SWARN(...)                                                      \
  logger(cerr, false, true, true, KRED, "\nWARNING: ", ##__VA_ARGS__, KNORM, "\n\n")
#define SDIE(...)                                                       \
  logger(cerr, true, true, true, KLRED, "\n[", upcxx::rank_me(), "] <", __FILENAME__ , ":", __LINE__, \
         "> ERROR: ", ##__VA_ARGS__, KNORM, "\n")


#define SLOG(...) do {                                              \
    logger(cout, false, true, true, ##__VA_ARGS__);                 \
    logger(_logstream, false, true, true, ##__VA_ARGS__);           \
  } while (0)

#define SLOG_VERBOSE(...) do {                                       \
    if (_verbose) logger(cout, false, true, true, ##__VA_ARGS__);    \
    logger(_logstream, false, true, true, ##__VA_ARGS__);           \
  } while (0)

#ifdef DEBUG
extern ofstream _dbgstream;
#define DBG(...) do {                                                   \
    if (_dbgstream) {                                                   \
      logger(_dbgstream, false, false, true, "<", __FILENAME__, ":", __LINE__, "> ", ##__VA_ARGS__); \
    }                                                                   \
  } while(0)
#else
#define DBG(...)
#endif

static double get_free_mem_gb(void) {
  string buf;
  ifstream f("/proc/meminfo");
  double mem_free = 0;
  while (!f.eof()) {
    getline(f, buf);
    if (buf.find("MemFree") == 0 || buf.find("Buffers") == 0 || buf.find("Cached") == 0) {
      stringstream fields;
      string units;
      string name;
      double mem;
      fields << buf;
      fields >> name >> mem >> units;
      if (units[0] == 'k') mem /= ONE_MB;
      mem_free += mem;
    }
  }
  return mem_free;
}

class IntermittentTimer {

  std::chrono::time_point<std::chrono::high_resolution_clock> t;
  double t_elapsed, t_interval;
  string name, interval_label;
public:
  IntermittentTimer(const string &name, string interval_label = "") : name{name}, interval_label{interval_label} {
    t_elapsed = 0;
    t_interval = 0;
  }

  void done_barrier() {
    auto max_t_elapsed = upcxx::reduce_one(t_elapsed, upcxx::op_fast_max, 0).wait();
    auto avg_t_elapsed = upcxx::reduce_one(t_elapsed, upcxx::op_fast_add, 0).wait() / upcxx::rank_n();
    SLOG_VERBOSE(KLCYAN, "--- Elapsed time for ", name, ": ", std::setprecision(2), std::fixed, " avg ", avg_t_elapsed,
                 " s max ", max_t_elapsed, " s balance ", (avg_t_elapsed / max_t_elapsed), " ---\n", KNORM);
    DBG("--- Elapsed time for ", name, ": ", std::setprecision(2), std::fixed, t_elapsed, " s ---\n");
  }

  void done() {
    SLOG_VERBOSE(KLCYAN, "--- Elapsed time for ", name, ": ", std::setprecision(2), std::fixed, t_elapsed, " s ---\n");
    DBG("--- Elapsed time for ", name, ": ", std::setprecision(2), std::fixed, t_elapsed, " s ---\n");
  }

  string get_final() {
    ostringstream os;
    os << name << ": " << std::setprecision(2) << std::fixed << t_elapsed;
    return os.str();
  }

  void start() {
    if (!interval_label.empty() && !_verbose) SLOG(KBLUE, std::left, std::setw(40), interval_label + ":", KNORM);
    t = std::chrono::high_resolution_clock::now();
  }

  void stop() {
    std::chrono::duration<double> interval = std::chrono::high_resolution_clock::now() - t;
    t_interval = interval.count();
    t_elapsed += t_interval;
    if (!interval_label.empty() && !_verbose) SLOG(KBLUE, std::setprecision(2), std::fixed, t_interval, " s", KNORM, "\n");
  }

  double get_interval() {
    return t_interval;
  }
};


class Timer {
  std::chrono::time_point<std::chrono::high_resolution_clock> t;
  string name;
  double init_free_mem;
  bool always_show;
public:
  Timer(const string &name, bool always_show=false) : always_show(always_show) {
    t = std::chrono::high_resolution_clock::now();
    this->name = name;
    if (!upcxx::rank_me()) init_free_mem = get_free_mem_gb();
    if (always_show) SLOG(KLCYAN, "-- ", name, " (", init_free_mem, " GB free) --\n", KNORM);
    else SLOG_VERBOSE(KLCYAN, "-- ", name, " (", init_free_mem, " GB free) --\n", KNORM);
  }

  ~Timer() {
    std::chrono::duration<double> t_elapsed = std::chrono::high_resolution_clock::now() - t;
    DBG(KLCYAN, "-- ", name, " took ", std::setprecision(2), std::fixed, t_elapsed.count(), " s --\n", KNORM);
    upcxx::barrier();
    t_elapsed = std::chrono::high_resolution_clock::now() - t;
    if (always_show) {
      SLOG(KLCYAN, "-- ", name, " took ", std::setprecision(2), std::fixed, t_elapsed.count(), " s (used ",
           (init_free_mem - get_free_mem_gb()), " GB) --\n", KNORM);
    } else {
      SLOG_VERBOSE(KLCYAN, "-- ", name, " took ", std::setprecision(2), std::fixed, t_elapsed.count(), " s (used ",
                   (init_free_mem - get_free_mem_gb()), " GB) --\n", KNORM);
    }
  }
};

inline string perc_str(int64_t num, int64_t tot) {
  ostringstream os;
  os.precision(2);
  os << std::fixed;
  os << num << " (" << 100.0 * num / tot << "%)";
  return os.str();
}

inline std::vector<string> split(const string &s, char delim) {
  std::vector<string> elems;
  std::stringstream ss(s);
  string token;
  while (std::getline(ss, token, delim)) elems.push_back(token);
  return elems;
}

inline void replace_spaces(string &s) {
  for (int i = 0; i < s.size(); i++)
    if (s[i] == ' ') s[i] = '_';
}

inline char comp_nucleotide(char ch) {
  switch (ch) {
      case 'A': return 'T';
      case 'C': return 'G';
      case 'G': return 'C';
      case 'T': return 'A';
      case 'N': return 'N';
      case '0': return '0';
      default: DIE("Illegal char in revcomp of '", ch, "'\n");
  }
  return 0;
}

inline string revcomp(const string &seq) {
  string seq_rc = "";
  seq_rc.reserve(seq.size());
  for (int i = seq.size() - 1; i >= 0; i--) seq_rc += comp_nucleotide(seq[i]);
  return seq_rc;
}

// returns 1 when it created the directory, 0 otherwise, -1 if there is an error
inline int check_dir(const char *path) {
  if (0 != access(path, F_OK)) {
    if (ENOENT == errno) {
      // does not exist
      // note: we make the directory to be world writable, so others can delete it later if we
      // crash to avoid cluttering up memory
      mode_t oldumask = umask(0000);
      if (0 != mkdir(path, 0777) && 0 != access(path, F_OK)) {
        umask(oldumask);
        fprintf(stderr, "Could not create the (missing) directory: %s (%s)", path, strerror(errno));
        return -1;
      }
      umask(oldumask);
    }
    if (ENOTDIR == errno) {
      // not a directory
      fprintf(stderr, "Expected %s was a directory!", path);
      return -1;
    }
  } else {
    return 0;
  }
  return 1;
}

// replaces the given path with a rank based path, inserting a rank-based directory
// example:  get_rank_path("path/to/file_output_data.txt", rank) -> "path/to/per_rank/<rankdir>/<rank>/file_output_data.txt"
// of if rank == -1, "path/to/per_rank/file_output_data.txt"
inline bool get_rank_path(string &fname, int rank) {
  char buf[MAX_FILE_PATH];
  strcpy(buf, fname.c_str());
  int pathlen = strlen(buf);
  char newPath[MAX_FILE_PATH*2+50];
  char *lastslash = strrchr(buf, '/');
  int checkDirs = 0;
  int thisDir;
  char *lastdir = NULL;

  if (pathlen + 25 >= MAX_FILE_PATH) {
    WARN("File path is too long (max: ", MAX_FILE_PATH, "): ", buf, "\n");
    return false;
  }
  if (lastslash) {
    *lastslash = '\0';
  }
  if (rank < 0) {
    if (lastslash) {
      snprintf(newPath, MAX_FILE_PATH*2+50, "%s/per_rank/%s", buf, lastslash + 1);
      checkDirs = 1;
    } else {
      snprintf(newPath, MAX_FILE_PATH*2+50, "per_rank/%s", buf);
      checkDirs = 1;
    }
  } else {
    if (lastslash) {
      snprintf(newPath, MAX_FILE_PATH*2+50, "%s/per_rank/%08d/%08d/%s", buf, rank / MAX_RANKS_PER_DIR, rank, lastslash + 1);
      checkDirs = 3;
    } else {
      snprintf(newPath, MAX_FILE_PATH*2+50, "per_rank/%08d/%08d/%s", rank / MAX_RANKS_PER_DIR, rank, buf);
      checkDirs = 3;
    }
  }
  strcpy(buf, newPath);
  while (checkDirs > 0) {
    strcpy(newPath, buf);
    thisDir = checkDirs;
    while (thisDir--) {
      lastdir = strrchr(newPath, '/');
      if (!lastdir) {
        WARN("What is happening here?!?!\n");
        return false;
      }
      *lastdir = '\0';
    }
    check_dir(newPath);
    checkDirs--;
  }
  fname = buf;
  return true;
}

inline string get_current_time() {
  auto t = std::time(nullptr);
  std::ostringstream os;
  os << std::put_time(localtime(&t), "%D %T");
  return os.str();
}

static string get_size_str(int64_t sz) {
  if (sz < 1024) return to_string(sz) + "B";
  double frac = 0;
  string units = "";
  if (sz >= ONE_GB * 1024l) {
    frac = (double)sz / (ONE_GB * 1024l);
    units = "TB";
  } else if (sz >= ONE_GB) {
    frac = (double)sz / ONE_GB;
    units = "GB";
  } else if (sz >= ONE_MB) {
    frac = (double)sz / ONE_MB;
    units = "MB";
  } else if (sz >= 1024) {
    frac = (double)sz / 1024;
    units = "KB";
  }
  ostringstream os;
  os << std::fixed << std::setprecision(2) << frac << units;
  return os.str();
}

static string remove_file_ext(const string &fname) {
  size_t lastdot = fname.find_last_of(".");
  if (lastdot == std::string::npos) return fname;
  return fname.substr(0, lastdot);
}

static string get_basename(const string &fname) {
  size_t i = fname.rfind('/', fname.length());
  if (i != string::npos) return(fname.substr(i + 1, fname.length() - i));
  return "";
}

static int64_t get_file_size(string fname) {
  struct stat s;
  if (stat(fname.c_str(), &s) != 0) return -1;
  return s.st_size;
}

#endif
