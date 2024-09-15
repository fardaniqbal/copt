/* copt.c - public domain command line option parsing library
   https://github.com/fardaniqbal/copt/ */
#include "copt.h"
#include <assert.h>
#include <string.h>

struct copt
copt_init(int argc, char **argv, int reorder)
{
  struct copt opt;
  opt.curopt = NULL;
  opt.argc = argc;
  opt.argv = argv;
  opt.idx = 0;
  opt.subidx = -1;
  opt.shortopt[0] = '\0';
  opt.reorder = !!reorder;
  return opt;
}

static int
copt_next_opt(struct copt *opt)
{
  int i;
  assert(opt->subidx < 0);
  if (opt->idx+1 >= opt->argc)
    return -1;
  for (i = ++opt->idx; i < opt->argc && opt->argv[i][0] != '-'; i++) {

  }
  /*if (opt->argv[opt->idx]*/
  /* TODO */
  return 0;
}

int
copt_done(struct copt *opt)
{
  if (opt->idx >= opt->argc)
    return 1;
  if (opt->subidx > 0) { /* in the middle of grouped short options */
    assert(opt->idx < opt->argc);
    if (opt->argv[opt->idx][opt->subidx] == '\0') /* end of group */
      opt->idx++, opt->subidx = -1;
    else {
      opt->shortopt[0] = '-';
      opt->shortopt[1] = opt->argv[opt->idx][opt->subidx];
      opt->shortopt[2] = '\0';
      opt->curopt = opt->shortopt;
    }
    return 0;
  }
  if (opt->subidx < 0) { /* done with last argv elem */
    int i = ++opt->idx;
    assert(i < opt->argc);
    if (!strcmp(opt->argv[i], "--")) /* just "--" means done */
      return opt->idx++, 1;
    if (opt->reorder)
      ; /* TODO: reorder argv here. */
    if (opt->argv[i][0] != '-') /* found non-option */
      return 1;
    if (opt->argv[i][1] == '\0') /* arg is just "-" */
      return 1;
    if (opt->argv[i][1] != '-') /* found short options */
      return (opt->subidx = 1), 0;
    assert(opt->argv[i][0] == '-' && opt->argv[i][1] == '-');
    opt->subidx = -1;
    opt->curopt = opt->argv[i];
    return 0; /* found long option */
  }
  if (opt->subidx > 0) { /* in the middle of grouped short options */
    int arglen = (int) strlen(opt->argv[opt->idx]);
    if (++opt->subidx < arglen) {
      /* TODO */
    }
  }
  /* TODO */
  return 0;
}

int
copt_opt(struct copt *opt, const char *option)
{
  const char *start, *end;
  char *arg = opt->argv[opt->idx];
  size_t arglen;
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
      return 1;
  }
  return 0;
}

char *
copt_arg(struct copt *opt)
{
  if (opt->subidx > 0) { /* in (possibly grouped) short option */
    int subidx = opt->subidx;
    opt->subidx = -1;
    if (opt->argv[opt->idx][subidx+1] != '\0')
      return opt->argv[opt->idx++] + subidx + 1;
    if (++opt->idx >= opt->argc || opt->argv[opt->idx][0] == '-')
      return NULL;
  } else {               /* in --long option */
    char *res;
    if ((res = strchr(opt->argv[opt->idx++], '=')) != NULL)
      return res+1;
    if (opt->idx >= opt->argc || opt->argv[opt->idx][0] == '-')
      return NULL;
  }
  assert(opt->idx < opt->argc);
  return opt->argv[opt->idx++];
}

int
copt_idx(struct copt *opt)
{
  return opt->idx;
}

char *
copt_curopt(struct copt *opt)
{
  return opt->curopt;
}
