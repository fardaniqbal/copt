# COPT - Command Line Option Parsing Library for C and C++

COPT is a simple, no-nonsense library for parsing command line options in C
and C++ programs.  Example usage:

```C
#include "copt.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
  const char *color = "auto";
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
      outfile = copt_arg(&opt);   /* get arg passed to option */
    } else if (copt_opt(&opt, "color=")) {
      /* ...handle --color[=OPTIONAL-ARG] ('=' is mandatory for arg)... */
      color = copt_arg(&opt);     /* get optional arg passed to option */
    } else {
      fprintf(stderr, "unknown option '%s'\n", copt_curopt(&opt));
      usage();
    }
  }
  infile = argv[copt_idx(&opt)];  /* handle non-option args */
  /* ... etc ... */
}
```

## Features

* [Public domain license](LICENSE).  I appreciate it if you credit me, but
  by no means is it required.  Do what you want with it.
* No heap allocations.
* Reentrant.  Option parsing context is stored in a `struct copt` object
  that _you_ declare.  Unlike most C/C++ command line option parsers, this
  one has no global state.  Comes in handy for sub-commands.
* Follows standard Unix command line conventions:
  * Handles short options (e.g. `-a`) and long options (e.g.
    `--like-this`).
  * Short options can be grouped (e.g. `-a -b -c` can be given as `-abc`).
  * Can _optionally_ handle mixed options and non-option args (e.g. `myprog
    --opt1 foo bar --opt2` will read both `--opt1` and `--opt2` as options
    while treating `foo` and `bar` as non-option arguments).
  * Can _optionally_ use `=` to pass args to long _and_ short options (e.g.
    `--longopt ARG`, `--longopt=ARG`, `-sARG`, `-s=ARG`, and `-s ARG` all
    work).
  * You can require a mandatory `=` for options with _optional arguments_
    (e.g. like GNU `ls`'s `--color` option).
  * Stops parsing options on `--`.
* Works with C and C++.
* Robust - includes comprehensive tests covering several permutations of
  short options, long options, option arguments, and non-option arguments.

## How to use

To use, copy `copt.c` and `copt.h` to anywhere in your project's directory
structure, and add `copt.h` to your project's include path.

## How to test

Run `make check`.  This will build and run binaries that test and verify
this library's functionality.
