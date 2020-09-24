SHELL='bash'

# Modify ARLROOT if you want to move this Makefile
# out of current directory.
ARLROOT=$(PWD)/../

include $(ARLROOT)/Makefile.ARLdefaults

SOURCES += $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp, %, $(SOURCES))

.PHONY: all test clean
.DEFAULT_GOAL := all

all: $(TARGETS)

test: all
	@for target in $(TARGETS) ; do \
		echo "R $$target $(BACKEND)" ;\
  	time $(TEST_RUN) ./$$target || (echo "$$target $(BACKEND) FAIL $$?"; exit 1) ;\
	done

clean:
	@rm -f $(TARGETS)