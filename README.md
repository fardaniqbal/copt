# COPT - Command Line Option Parsing Library for C and C++

Command line parsing should be basic functionality.  You shouldn't have to
pull a bunch of dependencies into your project just to give your users a
consistent CLI interface.

COPT is a no-nonsense, [single-file
library](https://github.com/nothings/single_file_libs) for parsing command
line options in C and C++ programs.  It aims to balance a practical feature
set with minimalism in implementation and calling code.

## Example usage

```C
#define COPT_IMPL
#include "copt.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
  const char *color = "auto";
  char *outfile = NULL;
  char *infile;
  struct copt opt = copt_init(argc, argv, 1);
  while (copt_next(&opt)) {
    if (copt_opt(&opt, "a")) {
      /* handle short option "-a" */
    } else if (copt_opt(&opt, "longopt")) {
      /* handle long option "--longopt" */
    } else if (copt_opt(&opt, "o|outfile")) {
      /* handle both short option "-o" and long option "--outfile" */
      outfile = copt_arg(&opt);   /* get MANDATORY arg passed to option */
    } else if (copt_opt(&opt, "c|color")) {
      /* handle --color[=OPTIONAL-ARG] (must have '=' for arg) */
      color = copt_oarg(&opt);    /* get OPTIONAL arg passed to option */
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

* [Public domain](LICENSE).  I appreciate it if you credit me, but by no
  means is it required.  You don't have to include the LICENSE file or
  reproduce it in your project's output.  Do what you want with COPT.
* Written in standard ISO C89, and works as both C and C++.
* No heap allocations.
* No dependencies on other libraries.
* Reentrant.  Option parsing context is stored in a `struct copt` object
  that _you_ declare.  Unlike most C/C++ command line option parsers, this
  one has no global state.  Comes in handy for sub-commands.
* Follows common Unix command line conventions:
  * Handles short options (e.g. `-a`) and long options (e.g.
    `--like-this`).
  * Short options can be grouped (e.g. `-a -b -c` can be given as `-abc`).
  * Can _optionally_ handle mixed options and non-option args (e.g. `myprog
    --opt1 foo bar --opt2` can read both `--opt1` and `--opt2` as options
    while treating `foo` and `bar` as non-option arguments).
  * Can _optionally_ use `=` to pass args to long _and_ short options (e.g.
    `--longopt ARG`, `--longopt=ARG`, `-sARG`, `-s=ARG`, and `-s ARG` all
    work).
  * You can require a mandatory `=` for options with _optional arguments_
    (e.g. like GNU `ls`'s `--color` option).
  * Stops parsing options on `--`.
* Comprehensive test suite.
  * Tests cover over one million permutations of short options, long
    options, option arguments, non-option arguments, and edge cases.
  * Tests C and C++ builds.

## How to use

Copy `copt.h` to anywhere in your project's directory structure, then in
_ONE_ C or C++ file, do `#define COPT_IMPL` before you `#include` it.

## How to test

Run `make check`.  This will build and run binaries that test and verify
this library's functionality.

## Alternatives

Why should you use COPT instead of another library?  [Here's a list of
other command line option parsing libraries](alternatives.md), and ways in
which they differ from COPT.  Depending on your use case, one of these
might be a better fit for your project than COPT.  But I doubt it ;-).

Below I've summarized the main issues I have with other command line
parsers (I elaborate further in the [aforementioned
link](alternatives.md)).

* [POSIX/UNIX
  `getopt()`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getopt.html) -
  not reentrant, not portable, and does not support `--long-options`.
* [GNU
  `getopt_long()`](https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Options.html) -
  not reentrant, not portable without pulling in the entirety of glibc, and
  required calling code is quite verbose.
* [Dropt (Deliberately Rudimentary
  Options)](https://github.com/jamesderlin/dropt) - requires options to
  preceed non-options, and uses heap allocation.
* [Suckless `arg.h`](https://git.suckless.org/st/file/arg.h.html) - not
  reentrant (though _almost_ there), does not support `--long-options`, and
  requires options to preceed non-options.
* [Argparse](https://github.com/Cofyc/argparse) - calls `exit()` on bad
  arguments with no way for caller to override such behavior.  (COPT also
  calls `exit()` but only _by default_, and lets caller set custom error
  handlers.)
* [Args](https://git.suckless.org/st/file/LICENSE.html) - substantial code
  size for a command line parser, and uses heap allocation.
* [James Theiler's OPT](https://salsa.debian.org/debian/opt) - includes the
  kitchen sink, and is generally just _way_ too overkill when all you want
  to do is parse a command line.

This is not a complete list.  If you know of any other option parsing
libraries that belong here, I'd greatly appreciate it if you let me know.

## References

* [POSIX Utility Argument
  Syntax](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html)
