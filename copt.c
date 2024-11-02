/* copt - public domain C/C++ command line option parsing library
   https://github.com/fardaniqbal/copt/ */
#include "copt.h"
#ifdef _MSC_VER
#  undef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS 1 /* proprietary MS stuff */
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* - copt implementation ----------------------------------------------- */

/* Return a copt context initialized to parse ARGC items from argument list
   ARGV.  If REORDER is true, ARGV may be reordered to allow mixing options
   with non-option args.  If REORDER is false, option parsing will stop at
   the first non-option arg in ARGV. */
struct copt
copt_init(int argc, char **argv, int reorder)
{
  struct copt opt;
  opt.curopt = NULL;
  opt.argc = argc;
  opt.argv = argv;
  opt.idx = 0;
  opt.subidx = 0;
  opt.argidx = 0;
  opt.noargfn = NULL;
  opt.noarg_aux = NULL;
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
  opt->argidx = 0;
  for (i = opt->idx; i < opt->argc; i++)
    if (argv[i][0] == '-' && argv[i][1] != '\0')
      break;
  if (i >= opt->argc || argv[i][0] != '-' || argv[i][1] == '\0')
    return;
  i++;
  copt_rotate_right(opt->argv + opt->idx, i - opt->idx);
  if (i >= opt->argc || opt->argv[i][0] != '-' || opt->argv[i][1] == '\0')
    opt->argidx = i;
  else
    opt->argidx = opt->argc;
}

/* Advance to next option.  Return true while options remain in the arg
   array passed to copt_init().  Return false when all options have been
   consumed, after which you'd call copt_idx() to get non-option args. */
int
copt_next(struct copt *opt)
{
  int i = opt->idx;
  opt->curopt = NULL;
  if (opt->idx >= opt->argc)
    return 0;
  if (opt->subidx > 0) {  /* inside grouped short options */
    char so;
    assert(i < opt->argc);
    assert(opt->argv[i][opt->subidx] != '\0');
    opt->subidx++;
    so = opt->argv[i][opt->subidx];
    if (so != '\0')
      return (opt->curopt = copt_set_shortopt(opt, so)), 1;
    opt->subidx = 0; /* leaving short option group */
  }
  /* done with previous argv elem */
  i = ++opt->idx;
  assert(i <= opt->argc);
  if (i >= opt->argc)
    return 0;
  if (opt->reorder)
    copt_reorder_opt(opt);
  if (!strcmp(opt->argv[i], "--"))  /* just "--" means done */
    return opt->idx++, 0;
  if (opt->argv[i][0] != '-')       /* found non-option */
    return 0;
  if (opt->argv[i][1] == '\0')      /* arg is just "-" */
    return 0;
  if (opt->argv[i][1] != '-') {     /* entering short option group */
    opt->subidx = 1;
    opt->curopt = copt_set_shortopt(opt, opt->argv[i][1]);
  } else {                          /* found long option */
    assert(opt->argv[i][0] == '-' && opt->argv[i][1] == '-');
    opt->subidx = 0;
    opt->curopt = opt->argv[i];
  }
  return 1;
}

/* After copt_next() indicates more options remain, call this function to
   act on the current option.  Return true if the option matches OPTSPEC.
   OPTSPEC gives a list of options to check against as a "|"-delimited
   string.  E.g. OPTSPEC="F|f|foo" returns true when the current option is
   "-F", "-f", or "--foo", accounting for grouped short options. */
int
copt_opt(const struct copt *opt, const char *optspec)
{
  char *arg = opt->argv[opt->idx];
  const char *start, *end;
  size_t arglen;
  assert((arg && arg[0] == '-' && arg[1] != '\0') || !!!"not option");

  if (opt->subidx > 0) /* in (possibly grouped) short option */
    arg += opt->subidx, arglen = 1; 
  else                 /* in --long option */
    arg += 2, arglen = strlen(arg);
  if ((end = strchr(arg, '=')) != NULL) /* --opt=ARG form */
    arglen = arglen < (size_t) (end-arg) ? arglen : end-arg;

  /* Search for current arg in pipe-delimited optspec. */
  for (start = optspec; *start != '\0'; start = end + (*end != '\0')) {
    end = strchr(start, '|');
    end = end ? end : start + strlen(start);
    if ((size_t) (end-start) == arglen && !memcmp(arg, start, arglen))
      return 1;
  }
  return 0;
}

static char *
copt_noarg(const struct copt *opt)
{
  if (opt->noargfn)
    return opt->noargfn(opt, opt->noarg_aux);
  fprintf(stderr, "%s: option '%s' requires arg\n",
          opt->argv[0], copt_curopt(opt));
  exit(1);
}

static char *
copt_arg_impl(struct copt *opt, int arg_is_optional)
{
  int subidx = opt->subidx;
  int argidx = opt->argidx;
  char ch, *eq;
  opt->subidx = opt->argidx = 0;

  if (subidx > 0) {             /* in (possibly grouped) short option */
    if ((ch = opt->argv[opt->idx][subidx+1]) != '\0')
      return opt->argv[opt->idx] + subidx + 1 + (ch == '=');
  } else if ((eq = strchr(opt->argv[opt->idx], '=')) != NULL)
    return eq+1;                /* --option=ARG */
  if (arg_is_optional)
    return NULL;                /* optional arg must be in argv[idx] */
  if (argidx >= opt->argc)
    return copt_noarg(opt);     /* reordered opt, no arg available */
  if (argidx > opt->idx)        /* reordered opt, arg available */
    copt_rotate_right(opt->argv + opt->idx + 1, argidx - opt->idx);
  if (opt->idx+1 >= opt->argc || (opt->argv[opt->idx+1][0] == '-' &&
                                  opt->argv[opt->idx+1][1] != '\0'))
    return copt_noarg(opt);     /* not optarg if it's just "-" */
  assert(opt->idx+1 < opt->argc);
  return opt->argv[++opt->idx]; /* optarg is the next argv item */
}

/* After copt_opt() indicates you found an option, call this function if
   your option expects an argument.  Returns the arg given to the option
   matched by the last call to copt_opt(). */
char *copt_arg(struct copt *opt) { return copt_arg_impl(opt, 0); }

/* After copt_opt() indicates you found an option, call this function if
   your option expects an argument.  Returns the arg given to the option
   matched by the last call to copt_opt(), or NULL if no arg exists. */
char *copt_oarg(struct copt *opt) { return copt_arg_impl(opt, 1); }

/* After copt_next() indicates you've consumed all options, this function
   returns index of first non-option argument in the argv array with which
   the given copt was initialized.  In idiomatic usage, you'd call this
   after your copt_next() loop terminates to get non-option args. */
int copt_idx(const struct copt *opt) { return opt->idx; }

/* Return the option found by most recent call to copt_next().  To meet
   copt's goal of zero heap allocation, the returned string is valid _only_
   until the next call on the given copt object, and _only_ while the given
   copt is in scope.  Caller must make a copy if needed longer.  Intended
   use is to show an error message when encountering unknown options or
   option with missing argument, for which idiomatic usage typically
   doesn't require making a copy. */
char *copt_curopt(const struct copt *opt) { return opt->curopt; }

/* Make copt context OPT call NOARGFN with the given AUX when copt_arg()
   doesn't find an option's argument.  If copt_arg() doesn't find an
   option, then it will return NOARGFN's return value. */
void
copt_set_noargfn(struct copt *opt, copt_errfn *noargfn, void *aux)
{
  opt->noargfn = noargfn;
  opt->noarg_aux = aux;
}
