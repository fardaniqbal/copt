# Makefile for building test program.
CFLAGS += -std=gnu89 -Wall -pedantic-errors -Os -g3

# Add .exe to binary filenames if targeting Windows.
target_os ?= $(shell uname -o | tr [:upper:] [:lower:])
bin_suffix := $(and $(filter msys% mingw% cygwin% win%,$(target_os)),.exe)

all: copt-test$(bin_suffix)
check: copt-test$(bin_suffix)
	./$<
copt-test$(bin_suffix): copt.o copt-test.o
	$(CC) -o $@ $^ $(LDFLAGS)
copt-test.o: copt-test.c copt.h
copt.o: copt.c copt.h
clean:; rm -f copt-test$(bin_suffix) *.o
