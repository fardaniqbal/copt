/* copt.c - public domain command line option parsing library
   https://github.com/fardaniqbal/copt/ */
#include "copt.h"
#ifdef _MSC_VER
#  undef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS 1 /* proprietary MS stuff */
#endif
#include <assert.h>
#include <string.h>

#if 1
static void copt_dbg(const char *fmt, ...) {}
#else
# include <stdio.h>
# define copt_dbg \
  (printf("%s:%d: %s(): ", __FILE__, __LINE__, __func__), printf)
#endif

struct copt
copt_init(int argc, char **argv, int reorder)
{
  struct copt opt;
  opt.curopt = NULL;
  opt.argc = argc;
  opt.argv = argv;
  opt.idx = 0;
  opt.subidx = 0;
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

int
copt_done(struct copt *opt)
{
  int i = opt->idx;
  opt->curopt = NULL;
  copt_dbg("\n");
  copt_dbg("entering (idx=%d, subidx=%d)\n", opt->idx, opt->subidx);
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
  if (!strcmp(opt->argv[i], "--")) /* just "--" means done */
    return copt_dbg("found '--', done\n"), opt->idx++, 1;
  copt_dbg("reorder? %d\n", opt->reorder);
  if (opt->reorder)
    ; /* TODO: reorder argv here. */
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

int
copt_opt(struct copt *opt, const char *option)
{
  const char *start, *end;
  char *arg = opt->argv[opt->idx];
  size_t arglen;
  assert((arg && arg[0] == '-' && arg[1] != '\0') || !!!"not option");

  if (opt->subidx > 0) /* in (possibly grouped) short option */
    arg += opt->subidx, arglen = 1; 
  else                 /* in --long option */
    arg += 2, arglen = strlen(arg);
  if ((end = strchr(arg, '=')) != NULL)
    arglen = arglen < (size_t) (end-arg) ? arglen : end-arg;
  for (start = option; *start != '\0'; start = end + (*end != '\0')) {
    end = strchr(start, '|');
    end = end ? end : start + strlen(start);
    if (end-start == arglen && !memcmp(arg, start, arglen))
      return copt_dbg("matched '%s'\n", option), 1;
  }
  return 0;
}

char *
copt_arg(struct copt *opt)
{
  if (opt->subidx > 0) { /* in (possibly grouped) short option */
    int subidx = opt->subidx;
    opt->subidx = 0;
    if (opt->argv[opt->idx][subidx+1] != '\0')
      return opt->argv[opt->idx] + subidx + 1;
  } else {               /* in --long option */
    char *res;
    if ((res = strchr(opt->argv[opt->idx], '=')) != NULL)
      return res+1;
  }
  /* Don't mistake a single '-' as an option. */
  if (opt->idx+1 >= opt->argc || (opt->argv[opt->idx+1][0] == '-' &&
                                  opt->argv[opt->idx+1][1] != '\0'))
    return NULL;
  opt->idx++;
  assert(opt->idx < opt->argc);
  return opt->argv[opt->idx];
}

int copt_idx(struct copt *opt) { return opt->idx; }
char *copt_curopt(struct copt *opt) { return opt->curopt; }
