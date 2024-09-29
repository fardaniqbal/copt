/* copt.c - public domain command line option parsing library
   https://github.com/fardaniqbal/copt/ */
#include "copt.h"
#ifdef _MSC_VER
#  undef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS 1 /* proprietary MS stuff */
#endif
#include <assert.h>
#include <string.h>

/* - debug logging ----------------------------------------------------- */

#if 0 /* make this #if 0 to enable debug logging, #if 1 to disable */
static void copt_dbg_reset(void) {}
static void copt_dbg(const char *fmt, ...) { (void) fmt; }
static void copt_dbg_args(struct copt *opt) { (void) opt; }
char *copt_dbg_dump(void) { return NULL; }
#else /* debug logging */
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# ifdef __GNUC__
#   define coptfunc __extension__ __func__
# else
#   define coptfunc __func__
# endif
# define copt_dbg \
  (copt_dbg_marksrcpos(__FILE__, __LINE__, coptfunc), copt_dbg_printf)
# define copt_dbg_args \
  (copt_dbg_marksrcpos(__FILE__, __LINE__, coptfunc), copt_dbg_args_)

static char copt_dbg_buf[8192];
static size_t copt_dbg_pos;

static void copt_dbg_reset(void) { copt_dbg_pos = copt_dbg_buf[0] = 0; }

/* Append string to log buffer, which is returned by copt_dbg_dump(). */
static void
copt_dbg_puts(const char *s)
{
  static const char *const trunc = "... <debug output truncated>";
  size_t len = strlen(s);
  size_t remains = sizeof copt_dbg_buf - copt_dbg_pos - 1; /* -1 for \0 */
  size_t max_len = len < remains ? len : remains;
  assert(copt_dbg_pos + max_len < sizeof copt_dbg_buf);
  memcpy(copt_dbg_buf + copt_dbg_pos, s, max_len);
  copt_dbg_pos += max_len;
  if (len >= remains)
    strcpy(copt_dbg_buf + sizeof copt_dbg_buf - strlen(trunc) - 1, trunc);
  assert(copt_dbg_pos < sizeof copt_dbg_buf);
  copt_dbg_buf[copt_dbg_pos] = '\0';
}

# ifdef __GNUC__
__extension__ __attribute__((format(printf, 1, 2)))
# endif
static void
copt_dbg_printf(const char *fmt, ...)
{
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  copt_dbg_puts(buf);
}

static void
copt_dbg_marksrcpos(const char *file, int line, const char *func)
{
  char buf[32];
  snprintf(buf, sizeof buf, "%d", line);
  copt_dbg_puts(file);
  copt_dbg_puts(":");
  copt_dbg_puts(buf);
  copt_dbg_puts(": ");
  copt_dbg_puts(func);
  copt_dbg_puts("(): ");
}

/* Retruned buffer is malloc()-d; caller must free() it. */
char *
copt_dbg_dump(void)
{
  char *rv;
  char *end = (char *) memchr(copt_dbg_buf, '\0', sizeof copt_dbg_buf);
  if (!end)
    copt_dbg_buf[sizeof copt_dbg_buf - 1] = '\0';
  if (copt_dbg_pos > 0 && copt_dbg_buf[copt_dbg_pos-1] != '\n')
    copt_dbg_puts("\n");
  if ((rv = (char *) malloc(copt_dbg_pos+1)) == NULL) {
    fputs(copt_dbg_buf, stdout);
    puts("copt_dbg_dump(): not enough memory to alloc return buffer");
    exit(1);
  }
  assert(copt_dbg_pos < sizeof copt_dbg_buf);
  memcpy(rv, copt_dbg_buf, copt_dbg_pos+1);
  copt_dbg_reset();
  return rv;
}

static void
copt_dbg_args_(struct copt *opt)
{
  int i;
  for (i = 0; i < opt->argc; i++)
    copt_dbg_puts("'"), copt_dbg_puts(opt->argv[i]), copt_dbg_puts("' ");
  copt_dbg_puts("\n");
}
#endif /* end debug logging */

/* - copt implementation ----------------------------------------------- */

/* Return a copt context initialized to parse ARGC items from argument list
   ARGV.  If REORDER is true, ARGV may be reordered to allow mixing options
   with non-option args.  If REORDER is false, option parsing will stop at
   the first non-option arg in ARGV. */
struct copt
copt_init(int argc, char **argv, int reorder)
{
  struct copt opt;
  copt_dbg_reset();
  opt.curopt = NULL;
  opt.argc = argc;
  opt.argv = argv;
  opt.idx = 0;
  opt.subidx = 0;
  opt.argidx = 0;
  opt.shortopt[0] = '\0';
  opt.reorder = !!reorder;
  return opt;
}

static char *
copt_set_shortopt(struct copt *opt, char c)
{
  opt->shortopt[0] = c == '\0' ? '\0' : '-';
  opt->shortopt[1] = c;
  opt->shortopt[2] = '\0';
  return c == '\0' ? NULL : opt->shortopt;
}

/* Rotate ARGC items in array ARGV one index to the right. */
static void
copt_rotate_right(char **argv, size_t argc)
{
  char *arg;
  assert(argc > 0);
  arg = argv[argc-1];
  memmove(argv+1, argv, (argc-1) * sizeof *argv);
  *argv = arg;
}

static void
copt_reorder_opt(struct copt *opt)
{
  char **argv = opt->argv;
  int i;
  copt_dbg("entering reorder (argidx cur=%d, new=0)...\n", opt->argidx);
  opt->argidx = 0;
  for (i = opt->idx; i < opt->argc; i++)
    if (argv[i][0] == '-' && argv[i][1] != '\0')
      break;
  if (i >= opt->argc || argv[i][0] != '-' || argv[i][1] == '\0') {
    copt_dbg("skipping reorder\n");
    return;
  }
  i++;
  copt_rotate_right(opt->argv + opt->idx, i - opt->idx);
  copt_dbg("rotated args from %d to %d:\n", opt->idx, i-1);
  copt_dbg_args(opt);
  if (i >= opt->argc || opt->argv[i][0] != '-' || opt->argv[i][1] == '\0')
    opt->argidx = i;
  else
    opt->argidx = opt->argc;
  copt_dbg("set new argidx=%d\n", opt->argidx);
}

/* Advance to next option.  Return false while options remain in the arg
   array passed to copt_init().  Return true after all options have been
   consumed. */
int
copt_done(struct copt *opt)
{
  int i = opt->idx;
  opt->curopt = NULL;
  copt_dbg("\n");
  copt_dbg("*** entering (idx=%d, subidx=%d, argidx=%d)\n",
           opt->idx, opt->subidx, opt->argidx);
  if (opt->idx >= opt->argc)
    return 1;
  if (opt->subidx > 0) {  /* in the middle of grouped short options */
    char so;
    copt_dbg("in short options (idx=%d, subidx=%d)\n", i, opt->subidx);
    assert(i < opt->argc);
    assert(opt->argv[i][opt->subidx] != '\0');
    opt->subidx++;
    so = opt->argv[i][opt->subidx];
    copt_dbg("opt = '%c'\n", so);
    if (so != '\0')
      return (opt->curopt = copt_set_shortopt(opt, so)), 0;
    copt_dbg("leaving short options\n");
    opt->subidx = 0; /* leaving short option group */
  }
  /* done with previous argv elem */
  i = ++opt->idx;
  copt_dbg("checking new elem (idx=%d, argc=%d)\n", i, opt->argc);
  assert(i <= opt->argc);
  if (i >= opt->argc)
    return copt_dbg("i >= argc, done\n"), 1;
  copt_dbg("reorder? %d\n", opt->reorder);
  if (opt->reorder)
    copt_reorder_opt(opt);
  if (!strcmp(opt->argv[i], "--")) /* just "--" means done */
    return copt_dbg("found '--', done\n"), opt->idx++, 1;
  copt_dbg("checking for non-option\n");
  if (opt->argv[i][0] != '-') /* found non-option */
    return 1;
  copt_dbg("checking for '-'\n");
  if (opt->argv[i][1] == '\0') /* arg is just "-" */
    return 1;
  if (opt->argv[i][1] != '-') { /* entering short option group */
    copt_dbg("entering short option group\n");
    opt->subidx = 1;
    opt->curopt = copt_set_shortopt(opt, opt->argv[i][1]);
  } else {                      /* found long option */
    copt_dbg("found long option\n");
    assert(opt->argv[i][0] == '-' && opt->argv[i][1] == '-');
    opt->subidx = 0;
    opt->curopt = opt->argv[i];
  }
  copt_dbg("curopt = '%s'\n", copt_curopt(opt));
  return 0;
}

/* After copt_done() indicates more options remain, call this function to
   act on the next option.  Return true if next option matches OPTSPEC.
   OPTSPEC gives the list of options to check against as a "|"-delimited
   string.  (e.g. OPTSPEC="F|f|foo" returns true when next option is "-F",
   "-f", or "--foo", accounting for grouped short options. */
int
copt_opt(const struct copt *opt, const char *optspec)
{
  const char *start, *end;
  char *arg = opt->argv[opt->idx];
  size_t arglen;
  assert((arg && arg[0] == '-' && arg[1] != '\0') || !!!"not option");

  if (opt->subidx > 0) /* in (possibly grouped) short option */
    arg += opt->subidx, arglen = 1; 
  else                 /* in --long option */
    arg += 2, arglen = strlen(arg);
  if ((end = strchr(arg, '=')) != NULL) /* --opt=ARG form */
    arglen = arglen < (size_t) (end-arg) ? arglen : end-arg;
  for (start = optspec; *start != '\0'; start = end + (*end != '\0')) {
    end = strchr(start, '|');
    end = end ? end : start + strlen(start);
    if ((size_t) (end-start) == arglen && !memcmp(arg, start, arglen))
      return copt_dbg("found matching opt '%s'\n", arg), 1;
  }
  return 0;
}

/* After copt_opt() indicates you found an option, call this function if
   your option expects an argument.  Returns the arg given to the option
   matched by the last call to copt_opt(), or NULL if no arg exists. */
char *
copt_arg(struct copt *opt)
{
  int subidx = opt->subidx;
  int argidx = opt->argidx;
  char ch, *eq;
  opt->subidx = opt->argidx = 0;

  if (subidx > 0) {        /* in (possibly grouped) short option */
    if ((ch = opt->argv[opt->idx][subidx+1]) != '\0')
      return opt->argv[opt->idx] + subidx + 1 + (ch == '=');
  } else if ((eq = strchr(opt->argv[opt->idx], '=')) != NULL)
    return eq+1;           /* --long-option=ARG */
  if (argidx >= opt->argc) /* reordered opt, no arg available */
    return NULL;
  if (argidx > opt->idx)   /* reordered opt, arg available */
    copt_rotate_right(opt->argv + opt->idx + 1, argidx - opt->idx);
  if (opt->idx+1 >= opt->argc || (opt->argv[opt->idx+1][0] == '-' &&
                                  opt->argv[opt->idx+1][1] != '\0'))
    return NULL; /* not optarg if starts with '-' but isn't _only_ '-' */
  opt->idx++;
  assert(opt->idx < opt->argc);
  return opt->argv[opt->idx];
}

/* After copt_done() indicates you've consumed all options, this function
   returns index of first non-option argument in the argv array with which
   the given copt was initialized.  In idiomatic usage, you'd call this
   after your copt_done() loop terminates to get non-option args. */
int copt_idx(const struct copt *opt) { return opt->idx; }

/* Return the option found by most recent call to copt_done().  To meet
   copt's goal of zero heap allocation, the returned string is valid _only_
   until the next call on the given copt object, and _only_ while the given
   copt is in scope.  Make a copy if you need it longer.  Intended use is
   to show an error message when encountering unknown options, for which
   idiomatic usage typically doesn't require making a copy. */
char *copt_curopt(const struct copt *opt) { return opt->curopt; }
