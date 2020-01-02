SHELL='bash'

##################### ARL part

# XXX: Modify ARLROOT if you move this Makefile
#      out of an examples/* directory.
ARLROOT=$(PWD)/../../../../

PLATFORM = $(shell echo $(ARL_PLATFORM) | tr '[:lower:]' '[:upper:]')

TIMER_CMD=time

BACKEND=GASNET_EX
ifeq ($(PLATFORM), CORI)
include $(gasnet_prefix)/include/aries-conduit/aries-par.mak
CXX = CC
else
include $(gasnet_prefix)/include/mpi-conduit/mpi-par.mak
CXX = mpic++
endif

ARLFLAGS = -lpthread $(GASNET_CXXCPPFLAGS) $(GASNET_CXXFLAGS) $(GASNET_LDFLAGS) $(GASNET_LIBS) -DGASNET_EX -I$(ARLROOT)

ARL_RUN=mpirun -n 4

CXXFLAGS = -std=gnu++17 $(ARLFLAGS)

##################### UPCXX part

CFLAGS = --std=c++11
# upcxx-meta PPFLAGS are really CFLAGS to be used during compilation
# upcxx-meta LDFLAGS are really CFLAGS to be used during linking
# upcxx-meta LIBFLAGS are really a comsbination of LDLIBS and LDFLAGS

UPCXXFLAGS = `upcxx-meta PPFLAGS` `upcxx-meta LDFLAGS`
LDFLAGS = `upcxx-meta LIBFLAGS`

############################################################3

TARGETS := kmer_hash_agg kmer_hash_upcxx_rpc kmer_hash_upcxx_rdma
all: $(TARGETS)

kmer_hash_naive: kmer_hash_naive.cpp kmer_t.hpp pkmer_t.hpp packing.hpp read_kmers.hpp
	@echo "C $@ $(BACKEND)"
	@time $(CXX) kmer_hash_naive.cpp -o kmer_hash_naive $(CXXFLAGS) || echo "$@ $(BACKEND) BUILD FAIL"

kmer_hash_agg: kmer_hash_agg.cpp kmer_t.hpp pkmer_t.hpp packing.hpp read_kmers.hpp
	@echo "C $@ $(BACKEND)"
	@time $(CXX) kmer_hash_agg.cpp -o kmer_hash_agg $(CXXFLAGS) || echo "$@ $(BACKEND) BUILD FAIL"

kmer_hash_upcxx_rpc: kmer_hash_upcxx_rpc.cpp kmer_t.hpp pkmer_t.hpp packing.hpp read_kmers.hpp hash_map_upcxx_rpc.hpp butil.hpp
	$(CXX) $(CFLAGS) kmer_hash_upcxx_rpc.cpp -o kmer_hash_upcxx_rpc $(UPCXXFLAGS) $(LDFLAGS)

kmer_hash_upcxx_rdma: kmer_hash_upcxx_rdma.cpp kmer_t.hpp pkmer_t.hpp packing.hpp read_kmers.hpp hash_map_upcxx_rdma.hpp butil.hpp
	$(CXX) $(CFLAGS) kmer_hash_upcxx_rdma.cpp -o kmer_hash_upcxx_rdma $(UPCXXFLAGS) $(LDFLAGS)

%: %.cpp kmer_t.hpp pkmer_t.hpp packing.hpp read_kmers.hpp
	@echo "C $@ $(BACKEND)"
	@time $(CXX) $< -o $@ $(CXXFLAGS) || echo "$@ $(BACKEND) BUILD FAIL"

clear:
	@rm -fv *.dat my_solution.txt

clean:
	@rm -fv $(TARGETS) *.dat my_solution.txt