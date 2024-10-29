# COPT - Command Line Option Parsing Library for C and C++

Command line parsing should be basic functionality.  You shouldn't have to
pull a bunch of dependencies into your project just to give your users a
consistent CLI interface.

COPT is a simple, no-nonsense library for parsing command line options in C
and C++ programs.

## Example usage

```C
#include "copt.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
  const char *color = "auto";
  char *outfile = NULL;
  char *infile;
  struct copt opt = copt_init(argc, argv, 1);
  while (copt_next(&opt)) {
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
* Written in standard ISO C89, and works as both C and C++.
* No heap allocations.
* No dependencies on other libraries.
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
* Comprehensive test suite.
  * Tests cover over one million permutations of short options, long
    options, option arguments, non-option arguments, and edge cases.
  * Tests C and C++ builds.

## How to use

To use, copy `copt.c` and `copt.h` to anywhere in your project's directory
structure, and add `copt.h` to your project's include path.

## How to test

Run `make check`.  This will build and run binaries that test and verify
this library's functionality.

## Alternatives

Why should you use COPT instead of another library?  Here are some other
command line option parsing libraries, and some ways in which they differ
from COPT.  Depending on your use case, one of these might be a better fit
for your project than COPT.  But I doubt it ;-).

* [POSIX/UNIX
  `getopt()`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getopt.html) - the "standard" UNIX command line parser.
  - Available by default on most UNIX platforms.
  - Typically not available at all on non-UNIX platforms.
  - Does not support long options (`--like-this`).
  - Behavior varies wildly accross different platforms.
    - E.g. GNU `getopt` allows options and their arguments to be mixed with
      non-option arguments, but most implementations do not.
  - Not reentrant due to its use of global variables.
  - Typical implementations are tied to an entire C library, which makes it
    impractical to pull into your project as a stand-alone library.

* [GNU
  `getopt_long()`](https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Options.html) - glibc's feature-rich long-option parser.
  - Available by default on most _(but not all)_ Linux distributions.
  - Rarely available on non-GNU systems.
  - Using `getopt_long` requires fairly verbose (and subjectively ugly)
    code for something as basic as command line parsing.
  - Not reentrant due to its use of global variables.
  - Tied to GNU libc, which makes it impractical (if not impossible) to
    pull into your project as a stand-alone library.
  - `getopt_long` is licensed under
    [LGPL](https://www.gnu.org/licenses/lgpl-3.0.html).  COPT is [public
    domain/unlicense](https://unlicense.org).

* [Dropt (Deliberately Rudimentary
  Options)](https://github.com/jamesderlin/dropt) - dropt actually has
  many similarities to COPT in terms of features and goals.  Differences:
  - Dropt automatically provides syntax sugar for boolean flags (e.g.
    `--flag` is equivalent to `--flag=1`).  COPT can handle boolean flags
    the same way, but doesn't provide the automatic syntax sugar.
  - Dropt does not support concatenating options with their arguments (e.g.
    cannot do `-oARG` with dropt).
  - Dropt requires all options and their arguments to preceed non-option
    arguments (e.g. cannot do `myprog ARG1 ARG2 --foo=val --bar`).
  - Dropt uses heap allocations, requiring caller to add additional code to
    check for allocation failures.  COPT uses _zero_ heap allocations.
  - Dropt uses [zlib/libpng license](http://opensource.org/licenses/Zlib).
    COPT is [public domain/unlicense](https://unlicense.org).

* [Suckless `arg.h`](https://git.suckless.org/st/file/arg.h.html) - command
  line parser used by many [suckless.org](https://suckless.org) programs.
  I really like this one, but it lacks certain features I wanted.
  - As a 50-line header implemented entirely with macros, Suckless's
    `arg.h` is arguably the most elegant command line parser I've seen,
    given its limited features.
    - See [st's `main()`](https://git.suckless.org/st/file/x.c.html#l2046)
      for a real-world usage example.
  - `arg.h` does not support long options (`--like-this`).
  - `arg.h` requires all options and their arguments to preceed non-option
    arguments (e.g. cannot do `myprog ARG1 ARG2 -a -b`).
  - `arg.h` does not support specifying option args with `=` (e.g. can do
    `-o ARG` and `-oARG`, but not `-o=ARG`).
  - `arg.h` is not reentrant due to its use of a global variable.
  - `arg.h`'s macros define local variables whose names may collide with
    your own variable names.
  - `arg.h` uses [MIT
    license](https://git.suckless.org/st/file/LICENSE.html).  COPT is
    [public domain/unlicense](https://unlicense.org).

* [Argparse](https://github.com/Cofyc/argparse) - pretty nice one here.
  Quite similar to COPT in terms of functionality.
  - Argparse uses C99 constructs (e.g. `...` in macro arguments).  COPT
    sticks to C89, since we still have a certain widely-used compiler
    vendor that doesn't support C99.
  - Calls `exit()` on error.  COPT lets caller handle errors.
  - Argparse uses [MIT
    license](https://git.suckless.org/st/file/LICENSE.html).  COPT is
    [public domain/unlicense](https://unlicense.org).

* [Args](https://git.suckless.org/st/file/LICENSE.html) from Darren
  Mulholland - feature-rich command line parsing API.
  - Args supports options with multiple arguments.  COPT does not, unless
    you use, e.g., comma-seperated arguments.
  - Code size is a bit substantial for a command line parsing library,
    including its own vector and hashmap implementations.
  - Args does not support concatenating options with their arguments (e.g.
    cannot do `-oARG`).
  - Args uses heap allocation, requiring caller to add additional code to
    check for allocation failures.  COPT uses _zero_ heap allocations.
  - Args is licensed under
    [0BSD](https://github.com/dmulholl/args/blob/master/license.txt).  COPT
    is [public domain/unlicense](https://unlicense.org).

This is not a comprehensive list.  If you know of any other option parsing
libraries that belong here, I'd greatly appreciate it if you let me know.

## References

* [POSIX Utility Argument
  Syntax](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html)
