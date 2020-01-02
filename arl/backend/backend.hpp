//
// Created by Jiakun Yan on 12/31/19.
//

#ifndef ARL_BACKEND_HPP
#define ARL_BACKEND_HPP

namespace arl {

  namespace backend {
    rank_t my_proc;
    rank_t my_nprocs;
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
      return my_nprocs;
    }

    inline void barrier() {
      gex_Event_t event = gex_Coll_BarrierNB(tm, 0);
      gex_Event_Wait(event);
    }

    inline void init(uint64_t shared_segment_size = 256, bool thread_safe = false) {
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
      my_nprocs = gex_System_QueryJobSize();

      gasnet_seginfo = (gasnet_seginfo_t*) malloc(sizeof(gasnet_seginfo_t) * my_nprocs);
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
    }

    template <typename ...Args>
    void print(std::string format, Args... args) {
      fflush(stdout);
      barrier();
      if (rank_me() == 0) {
        printf(format.c_str(), args...);
      }
      fflush(stdout);
      barrier();
    }
  }
}

#endif //ARL_BACKEND_HPP
