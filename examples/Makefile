SHELL='bash'

# Modify ARLROOT if you want to move this Makefile
# out of current directory.
ARLROOT=$(PWD)/../

include $(ARLROOT)/Makefile.ARLdefaults

SOURCES += $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp, %, $(SOURCES))

.PHONY: all clean
.DEFAULT_GOAL := all

all: $(TARGETS)

%: %.cpp
	@echo "C $@ $(BACKEND)"
	@time $(CXX) -o $@ $^ $(CXXFLAGS) || echo "$@ $(BACKEND) BUILD FAIL"

clean:
	@rm -f $(TARGETS)