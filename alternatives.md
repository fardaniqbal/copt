## Alternatives

Why should you use COPT instead of another library?  Here are some other
command line option parsing libraries, and ways in which they differ from
COPT.  Depending on your use case, one of these might be a better fit for
your project than COPT.  But I doubt it ;-).

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
  - License: [LGPL](https://www.gnu.org/licenses/lgpl-3.0.html).
  - Available by default on most _(but not all)_ Linux distributions.
  - Rarely available on non-GNU systems.
  - Using `getopt_long` requires fairly verbose (and subjectively ugly)
    code for something as basic as command line parsing.
  - Not reentrant due to its use of global variables.
  - Tied to GNU libc, which makes it impractical (if not impossible) to
    pull into your project as a stand-alone library.

* [Dropt (Deliberately Rudimentary
  Options)](https://github.com/jamesderlin/dropt) - dropt actually has
  many similarities to COPT in terms of features and goals.  Differences:
  - License: [zlib/libpng](http://opensource.org/licenses/Zlib).
  - Dropt automatically provides syntax sugar for boolean flags (e.g.
    `--flag` is equivalent to `--flag=1`).  COPT can handle boolean flags
    the same way, but doesn't provide the automatic syntax sugar.
  - Dropt does not support concatenating options with their arguments (e.g.
    cannot do `-oARG` with dropt).
  - Dropt requires all options and their arguments to preceed non-option
    arguments (e.g. cannot do `myprog ARG1 ARG2 --foo=val --bar`).
  - Dropt uses heap allocations, requiring caller to add additional code to
    check for allocation failures.  COPT uses _zero_ heap allocations.

* [Suckless `arg.h`](https://git.suckless.org/st/file/arg.h.html) - command
  line parser used by many [suckless.org](https://suckless.org) programs.
  I really like this one, but it lacks certain features I wanted.
  - License: [MIT](https://git.suckless.org/st/file/LICENSE.html).
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

* [Argparse](https://github.com/Cofyc/argparse) - pretty nice one here.
  Quite similar to COPT in terms of functionality.
  - License: [MIT](https://git.suckless.org/st/file/LICENSE.html).
  - Argparse uses C99 constructs (e.g. `...` in macro arguments).  COPT
    sticks to C89, since we still have a certain widely-used compiler
    vendor that doesn't support C99.
  - Calls `exit()` on error.  COPT calls `exit()` _by default_, but also
    lets caller set custom error handlers.

* [Args](https://git.suckless.org/st/file/LICENSE.html) from Darren
  Mulholland - feature-rich command line parsing API.
  - License:
    [0BSD](https://github.com/dmulholl/args/blob/master/license.txt).
  - Args supports options with multiple arguments.  COPT does not, unless
    you use, e.g., comma-seperated arguments.
  - Code size is a bit substantial for a command line parsing library,
    including its own vector and hashmap implementations.
  - Args does not support concatenating options with their arguments (e.g.
    cannot do `-oARG`).
  - Args uses heap allocation, requiring caller to add additional code to
    check for allocation failures.  COPT uses _zero_ heap allocations.

* [James Theiler's OPT](https://salsa.debian.org/debian/opt) - command line
  parser with a kitchen sink approach to features, including built-in
  functionality to read options from file, and run as an interactive menu.
  Polar opposite of the minimalism in [Suckless
  `arg.h`](https://git.suckless.org/st/file/arg.h.html).
  - License:
    [GPL](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
  - OPT is not reentrant due to its use of global variables.
  - OPT uses heap allocations, _and does not check for allocation
    failures_.  COPT uses _zero_ heap allocations.
  - Requires many external dependencies, including GNU autotools.
  - Substantial code size due to kitchen sink approach, including its own
    vector implementation, among other things.
    - Makes OPT harder to bundle into your project, unlike COPT where you
      just copy the header into your project's directory.
  - Somewhat awkward API involving several caller-specified callbacks.

This is not a complete list.  If you know of any other option parsing
libraries that belong here, I'd greatly appreciate it if you let me know.
