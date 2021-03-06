SHELL='bash'

# Modify ARLROOT if you want to move this Makefile
# out of current directory.
ARLROOT=$(PWD)/../../

PLATFORM = $(shell echo $(HOSTNAME) | tr '[:lower:]' '[:upper:]')

TIMER_CMD=time

BACKEND=GASNET_EX
# make on Cori
ifeq (CORI,$(findstring CORI,$(PLATFORM)))
    include $(gasnet_prefix)/include/aries-conduit/aries-par.mak
    CXX = CC
# make on docker
else
include $(gasnet_prefix)/include/mpi-conduit/mpi-par.mak
CXX = mpic++
endif

ARLFLAGS = -lpthread $(GASNET_CXXCPPFLAGS) $(GASNET_CXXFLAGS) $(GASNET_LDFLAGS) $(GASNET_LIBS) -DGASNET_EX -I$(ARLROOT)

CXXFLAGS = -std=gnu++17 $(ARLFLAGS)

##################### UPCXX part

CFLAGS = --std=c++11
# upcxx-meta PPFLAGS are really CFLAGS to be used during compilation
# upcxx-meta LDFLAGS are really CFLAGS to be used during linking
# upcxx-meta LIBFLAGS are really a comsbination of LDLIBS and LDFLAGS

UPCXXFLAGS = `upcxx-meta PPFLAGS` `upcxx-meta LDFLAGS`
LDFLAGS = -L/usr/common/ftg/upcxx/2019.9.4-snapshot-2020.01.15/hsw/gnu/PrgEnv-gnu-6.0.5-8.3.0/upcxx.O3.gasnet_seq.aries/lib -lupcxx -L/usr/common/ftg/upcxx/2019.9.4-snapshot-2020.01.15/hsw/gnu/PrgEnv-gnu-6.0.5-8.3.0/gasnet.opt/lib -lgasnet-aries-seq $(GASNET_LIBS)

############################################################3

SOURCES += $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp, %, $(SOURCES))

all: $(TARGETS)

upcxx_bw: upcxx_bw.cpp
	$(CXX) $(CFLAGS) $^ -o $@ $(UPCXXFLAGS) $(LDFLAGS)

%: %.cpp
	@echo "C $@ $(BACKEND)"
	@time $(CXX) -o $@ $^ $(CXXFLAGS) || echo "$@ $(BACKEND) BUILD FAIL"

TEST_RUN=mpirun -n 4

test: all
	@for target in $(TARGETS) ; do \
		echo "R $$target $(BACKEND)" ;\
  	time $(TEST_RUN) ./$$target || (echo "$$target $(BACKEND) FAIL $$?"; exit 1);\
	done

clean:
	@rm -f $(TARGETS)