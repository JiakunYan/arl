//
// Created by Jiakun Yan on 12/31/19.
//
/*
 * Acknowledgement:
 *   Most content in this file is based on the source code of
 *   the Berkeley Container Library (https://github.com/berkeley-container-library/bcl).
*/

#ifndef ARL_BACKEND_HPP
#define ARL_BACKEND_HPP

namespace arl {
namespace backend {
rank_t my_proc;
rank_t nprocs;
bool finalized;

gex_Client_t client;
gex_EP_t ep;
gex_TM_t tm;
const char* clientName = "ARL";

gasnet_seginfo_t* gasnet_seginfo;
uint64_t shared_segment_size;
void *smem_base_ptr;

//    extern inline void init_malloc();

inline rank_t rank_me() {
  return my_proc;
}

inline rank_t rank_n() {
  return nprocs;
}

inline void barrier() {
  gex_Event_t event = gex_Coll_BarrierNB(tm, 0);
  while (gex_Event_Test(event)) {
    progress();
  }
}

inline void init(uint64_t shared_segment_size, bool thread_safe) {
  shared_segment_size = 1024*1024*shared_segment_size;

  gex_Client_Init(&client, &ep, &tm, clientName, NULL, NULL, 0);

  if (thread_safe) {
#ifndef GASNET_PAR
    throw std::runtime_error("Need to use a par build of GASNet-EX");
#endif
  }

  gex_Segment_t segment;
  gex_Segment_Attach(&segment, tm, shared_segment_size);

  smem_base_ptr = gex_Segment_QueryAddr(segment);

  if (smem_base_ptr == NULL) {
    throw std::runtime_error("arl::backend: Could not allocate shared memory segment.");
  }

  my_proc = gex_System_QueryJobRank();
  nprocs = gex_System_QueryJobSize();

  gasnet_seginfo = (gasnet_seginfo_t*) malloc(sizeof(gasnet_seginfo_t) * nprocs);
  gasnet_getSegmentInfo(gasnet_seginfo, rank_n());

//      init_malloc();
//      init_atomics();

  finalized = false;

  barrier();
}

inline void finalize() {
  barrier();
//      finalize_atomics();
  free(gasnet_seginfo);
  finalized = true;
  gasnet_exit(0);
} // namespace backend

void print(const std::string& format) {
  fflush(stdout);
  barrier();
  if (rank_me() == 0) {
    printf("%s\n", format.c_str());
  }
  fflush(stdout);
  barrier();
}

template <typename T, typename ...Args>
void print(const std::string& format, T arg, Args... args) {
  fflush(stdout);
  barrier();
  if (rank_me() == 0) {
    printf(format.c_str(), arg, args...);
  }
  fflush(stdout);
  barrier();
}
}
}

#endif //ARL_BACKEND_HPP
