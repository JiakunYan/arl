SHELL='bash'

# Modify ARLROOT if you want to move this Makefile
# out of current directory.
ARLROOT=$(PWD)/../

PLATFORM = $(shell echo $(HOSTNAME) | tr '[:lower:]' '[:upper:]')

TIMER_CMD=time

BACKEND=GASNET_EX
ARLFLAGS = -lpthread $(GASNET_CXXCPPFLAGS) $(GASNET_CXXFLAGS) $(GASNET_LDFLAGS) $(GASNET_LIBS) -DGASNET_EX -I$(ARLROOT)
# make on Cori
ifeq (CORI,$(findstring CORI,$(PLATFORM)))
    include $(gasnet_prefix)/include/aries-conduit/aries-par.mak
    CXX = CC
# make on docker
else
include $(gasnet_prefix)/include/mpi-conduit/mpi-par.mak
CXX = mpic++
ARLFLAGS += -DARL_DEBUG
endif

CXXFLAGS = -std=gnu++17 $(ARLFLAGS)

SOURCES += $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp, %, $(SOURCES))

all: $(TARGETS)

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