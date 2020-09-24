SHELL='bash'

# Modify ARLROOT if you want to move this Makefile
# out of current directory.
ARLROOT=$(PWD)/../../

include $(ARLROOT)/Makefile.ARLdefaults

SOURCES += $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp, %, $(SOURCES))

.PHONY: all clean
.DEFAULT_GOAL := all

all: $(TARGETS)

clean:
	@rm -f $(TARGETS) *.arl_dlog*