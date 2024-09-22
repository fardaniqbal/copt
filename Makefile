# Makefile for building test program.
CC += -std=gnu89
CXX += -std=c++98
CFLAGS += -Wall -pedantic-errors -Os -g3

# Add .exe to binary filenames if targeting Windows.
target_os ?= $(shell uname -o | tr [:upper:] [:lower:])
bin_suffix := $(and $(filter msys% mingw% cygwin% win%,$(target_os)),.exe)
$(info bin_suffix = '$(bin_suffix)')

all: copt-test$(bin_suffix) copt-test-cpp$(bin_suffix)
check: copt-test$(bin_suffix) #copt-test-cpp$(bin_suffix)
	for i in $<; do ./$$i || exit 1; done
copt-test$(bin_suffix): copt.o copt-test.o
	$(CC) -o $@ $^ $(LDFLAGS)
copt-test-cpp(bin_suffix): copt-cpp.o copt-test-cpp.o
	$(CXX) -o $@ $^ $(LDFLAGS)
copt-test.o: copt-test.c copt.h
	$(CC) $< -o $@ -c $(CFLAGS)
copt.o: copt.c copt.h
	$(CC) $< -o $@ -c $(CFLAGS)
copt-test-cpp.o: copt-test.c copt.h
	$(CXX) $< -o $@ -c $(CFLAGS) $(CXXFLAGS)
copt-cpp.o: copt.c copt.h
	$(CXX) $< -o $@ -c $(CFLAGS) $(CXXFLAGS)
clean:; rm -f copt-test$(bin_suffix) copt-test-cpp$(bin_suffix) *.o
