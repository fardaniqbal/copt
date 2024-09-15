# COPT - Command Line Option Parsing Library for C and C++

COPT is a simple, no-nonsense library for parsing command line options in C
and C++ programs.  Example usage:

```C
#include "copt.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
  char *outfile = NULL;
  char *infile;
  struct copt opt = copt_init(argc, argv, 1);
  while (!copt_done(&opt)) {
    if (copt_opt(&opt, "a") {
      /* ...handle short option "-a"... */
    } else if (copt_opt(&opt, "longopt")) {
      /* ...handle long option "--longopt"... */
    } else if (copt_opt(&opt, "o|outfile")) {
      /* ...handle both short option "-o" and long option "--outfile"... */
      outfile = copt_arg(&copt);
    } else {
      fprintf(stderr, "unrecognized option '%s'\n", copt_curopt(&opt));
      usage();
    }
  }
  infile = argv[copt_idx(&opt)];
  /* ... etc ... */
}
```

## Features

* Public domain license.  I appreciate it if you credit me, but by no means
  is it required.  Do what you want with it.
* No heap allocations.
* Handles short options (e.g. `-a`) and long options (e.g. `--like-this`).
* Short options can be grouped (e.g. `-a -b -c` can be given as `-abc`).
* Can _optionally_ handle mixed options and non-option args (e.g. `myprog
  --opt1 foo bar --opt2` will read both `--opt1` and `--opt2`).
* Can use `=` to specify arguments to long options (e.g., both `--longopt
  ARG` and `--longopt=ARG` work).
* Reentrant.  Option parsing context is stored in a `struct copt` object
  that _you_ declare.  Unlike most C/C++ command line option parsers, this
  one has no global state.  Comes in handy for sub-commands.

## How to use

To use, copy `copt.c` and `copt.h` to anywhere in your project's directory
structure, and add `copt.h` to your project's include path.

## How to test

Run `make`.  This will generate a `copt-test` binary, which you can run to
test this library's functionality.
