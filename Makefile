# Makefile for building test program.
CC += -std=gnu89
CFLAGS += -Wall -pedantic-errors -Os -g3
CXXFLAGS += -std=c++98

# Add .exe to binary filenames if targeting Windows.
target_os ?= $(shell uname -s | tr [:upper:] [:lower:])
bin_suffix := $(and $(filter msys% mingw% cygwin% win%,$(target_os)),.exe)

all: copt-test$(bin_suffix) copt-test-cpp$(bin_suffix)

# Allow tests to run in parallel when using `make -j`.
check: check-copt-test check-copt-test-cpp
check-%: %$(bin_suffix); ./$<

copt-test$(bin_suffix): copt.o copt-test.o
	$(CC) -o $@ $^ $(LDFLAGS)
copt-test-cpp$(bin_suffix): copt-cpp.o copt-test-cpp.o
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o:     %.c copt.h; $(CC) -o $@ $(CFLAGS) -c $<
%-cpp.o: %.c copt.h; $(CXX) -x c++ -o $@ $(CFLAGS) $(CXXFLAGS) -c $<
clean:; rm -f copt-test$(bin_suffix) copt-test-cpp$(bin_suffix) *.o
