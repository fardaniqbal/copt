/* copt-test.c - tests for copt library
   https://github.com/fardaniqbal/copt/ */
#include "copt.h"
#ifdef _MSC_VER
#  undef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS 1 /* proprietary MS stuff */
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef NDEBUG
#include <assert.h>

#if 0
static void test_dbg(const char *fmt, ...) {}
#else
# define test_dbg \
  (printf("%s:%d: %s(): ", __FILE__, __LINE__, __func__), printf)
#endif

static size_t total_test_cnt;
static size_t failed_test_cnt;
static int test_line;
static char **fail_info; /* array of failed_test_cnt strings */

/* Make sure fail_info array has space for failed_test_cnt+1 items. */
static char *
logf_ensure_cap(void)
{
  char **info = fail_info;
  static size_t last_cnt;
  size_t cnt = failed_test_cnt + 1;
  if (!info)
    info = (char **) calloc(cnt, sizeof *fail_info);
  else
    info = (char **) realloc(info, cnt * sizeof *fail_info);
  if (!info)
    printf("%s:%d: out of memory\n", __FILE__, test_line), exit(1);
  if (cnt > last_cnt)
    info[cnt-1] = NULL;
  fail_info = info;
  last_cnt = cnt;
  return fail_info[failed_test_cnt];
}

/* Print message to current fail_info string, reallocating if necessary. */
# ifdef __GNUC__
__extension__ __attribute__((format(printf, 1, 2)))
# endif
static void
logf(const char *fmt, ...)
{
  /* Using strlen on every call isn't the most efficient implementation in
     terms of big-O, but should suffice for testing purposes. */
  char *cp, *buf = logf_ensure_cap();
  int rc, basecap, cap = 1;
  va_list ap;
  if (buf == NULL && (buf = (char *) calloc(1,1)) == NULL)
    printf("not enough memory for log\n"), exit(1);
  basecap = strlen(buf);
  cp = buf + basecap;

  /* Do this in a loop because MSVC's vsnprintf returns < 0 on trunc. */
  va_start(ap, fmt);
  while ((rc = vsnprintf(cp, cap, fmt, ap)) >= cap || rc < 0) {
    va_end(ap);
    /* Just ignore int overflow. */
    cap = rc >= 0 ? rc+1 : (basecap+cap) * 2 - basecap;
    if ((buf = (char *) realloc(buf, basecap + cap)) == NULL)
      printf("not enough memory for fmt '%s'\n", fmt), exit(1);
    cp = buf + basecap;
    va_start(ap, fmt);
  }
  va_end(ap);
  fail_info[failed_test_cnt] = buf;
}

/* Return a malloc-d array of the given string varargs. */
static char **
mkargv(const char *first, ...)
{
  char **arr;
  size_t cnt = 0, max_cnt = 1;
  va_list ap;

  if (!(arr = (char **) malloc(max_cnt * sizeof *arr)))
    goto no_mem;
  arr[cnt++] = (char *) first;
  va_start(ap, first);
  do {
    if (cnt >= max_cnt) {
      max_cnt = max_cnt * 2 + 1;
      if (!(arr = (char **) realloc(arr, max_cnt * sizeof *arr)))
        goto no_mem;
    }
  } while ((arr[cnt++] = va_arg(ap, char *)) != NULL);
  va_end(ap);
  return arr;

no_mem:
  printf("out of memory\n");
  exit(1);
}

enum argtype {
  ARGTYPE_NULL   = 0,
  ARGTYPE_OPT    = 1,
  ARGTYPE_OPTARG = 2,
  ARGTYPE_ARG    = 3,
  ARGTYPE_BADOPT = 4
};

static const char *
argtype_str(enum argtype type)
{
  switch (type) {
    case ARGTYPE_NULL:    return "(null)";
    case ARGTYPE_OPT:     return "OPT";
    case ARGTYPE_OPTARG:  return "OPTARG";
    case ARGTYPE_ARG:     return "ARG";
    case ARGTYPE_BADOPT:  return "BADOPT";
    default: printf("bad argtype %d\n", type); exit(1);
  }
}

struct arg {
  enum argtype type;
  const char *val;
};

/* Return true if two args are the same, including NULL values. */
static int
arg_eq(const struct arg *a1, const struct arg *a2)
{
  if (!a1 || !a2)
    return (a1 == NULL) == (a2 == NULL);
  if (a1->type != a2->type)
    return 0;
  if (a1->val && a2->val)
    return !strcmp(a1->val, a2->val);
  return (a1->val == NULL) && (a2->val == NULL);
}

struct testcase {
  size_t expect_cnt;
  size_t actual_cnt;
  struct arg expect[64];
  struct arg actual[64];
  size_t argc;
  char *argv[64];
  char *argv_copy[64]; /* because argv might get reordered */
};

/* Print formatted table of expected vs actual args to failure log. */
static void
testcase_dump(const struct testcase *tc)
{
  const int val_w = 27, type_w = 7;
  size_t i;
  logf("%-*s | %s\n", val_w+type_w+1, "EXPECTED", "ACTUAL");
  for (i = 0; i < (size_t) (val_w+type_w+1); i++) logf("-");
  logf(" | ");
  for (i = 0; i < (size_t) (val_w+type_w+1); i++) logf("-");
  logf("\n");

  for (i = 0; i < tc->expect_cnt || i < tc->actual_cnt; i++) {
    const char *ev = NULL, *et = "";
    const char *av = NULL, *at = "";
    if (i < tc->expect_cnt)
      ev = tc->expect[i].val, et = argtype_str(tc->expect[i].type);
    if (i < tc->actual_cnt)
      av = tc->actual[i].val, at = argtype_str(tc->actual[i].type);
    logf("%-*s %-*s | ", val_w, ev, type_w, et);
    logf("%-*s %-*s\n",  val_w, av, type_w, at);
  }
}

static char *
my_strdup(const char *str)
{
  size_t nbyte = str ? strlen(str) + 1 : 0;
  void *mem = nbyte ? malloc(nbyte) : NULL;
  if (mem != NULL || nbyte == 0)
    return (char *) memcpy(mem, str, nbyte);
  printf("failed to duplicate %lu-byte string '%s'\n", (long) nbyte, str);
  exit(1);
}

/* Call this on values actually returned by the option parser. */
static void
actual(struct testcase *tc, enum argtype type, const char *val)
{
  size_t i = tc->actual_cnt++;
  assert(i < sizeof tc->actual / sizeof *tc->actual);
  tc->actual[i].type = type;
  tc->actual[i].val = my_strdup(val);
}

/* Call this on what we _expect_ the command parser to return. */
static void
expect(struct testcase *tc, enum argtype type, const char *val)
{
  size_t i = tc->expect_cnt++;
  assert(i < sizeof tc->expect / sizeof *tc->expect);
  tc->expect[i].type = type;
  tc->expect[i].val = my_strdup(val);
}

#define actual_opt(tc, opt)       (actual(tc, ARGTYPE_OPT, opt))
#define actual_optarg(tc, optarg) (actual(tc, ARGTYPE_OPTARG, optarg))
#define actual_arg(tc, arg)       (actual(tc, ARGTYPE_ARG, arg))
#define actual_badopt(tc, opt)    (actual(tc, ARGTYPE_BADOPT, opt))
#define expect_opt(tc, opt)       (expect(tc, ARGTYPE_OPT, opt))
#define expect_optarg(tc, optarg) (expect(tc, ARGTYPE_OPTARG, optarg))
#define expect_arg(tc, arg)       (expect(tc, ARGTYPE_ARG, arg))
#define expect_badopt(tc, opt)    (expect(tc, ARGTYPE_BADOPT, opt))

static void
test_begin(struct testcase *tc, ...)
{
  const char *arg;
  va_list ap;
  memset(tc, 0, sizeof *tc);
  tc->argv[tc->argc++] = my_strdup("copt");
  va_start(ap, tc);
  while ((arg = va_arg(ap, char *)) != NULL) {
    assert(tc->argc < sizeof tc->argv / sizeof *tc->argv);
    tc->argv[tc->argc++] = my_strdup(arg);
  }
  va_end(ap);
}

#define test_begin (test_line = __LINE__, test_begin)

static void
test_addargs(struct testcase *tc, char **argv)
{
  while (*argv != NULL) {
    assert(tc->argc < sizeof tc->argv / sizeof *tc->argv);
    tc->argv[tc->argc++] = my_strdup(*argv++);
  }
}

/* Print args into NBYTE-size buffer DST, truncating if longer than
   MAX_WIDTH chars.  Return actual required size of DST, including \0. */
static size_t
sprint_args(char *dst, size_t nbyte,
            size_t argc, char **argv, size_t max_width)
{
  size_t i, cp;
  assert(dst != NULL || nbyte == 0);
  for (i = cp = 0; i < argc; cp += strlen(argv[i])+1, i++)
    if (cp + strlen(argv[i]) + 1 < nbyte)
      strcat(strcpy(dst+cp, argv[i]), " ");
    else if (cp < nbyte)
      memcpy(dst+cp, argv[i], nbyte-1-cp);
  if (4 <= max_width && max_width < cp && max_width < nbyte)
    strcpy(dst+max_width-4, "... ");
  if (max_width > 0)
    for (; cp < max_width && cp < nbyte-1; cp++)
      dst[cp] = ' ';
  if (nbyte)
    dst[nbyte-1 < cp ? nbyte-1 : cp] = '\0';
  return max_width+1 < cp ? max_width+1 : cp;
}

static void
logf_args(size_t argc, char **argv, size_t max_width)
{
  char buf[1024];
  sprint_args(buf, sizeof buf, argc, argv, max_width);
  logf("%s", buf);
}

static void
test_verify(struct testcase *tc)
{
  char buf[71], *tmp;
  size_t i;
  total_test_cnt++;
  sprint_args(buf, sizeof buf, tc->argc, tc->argv, sizeof buf-1);
  fputs(buf, stdout);

  if (tc->expect_cnt == tc->actual_cnt) {
    /* Check if actual args differ from expected args. */
    for (i = 0; i < tc->expect_cnt; i++)
      if (!arg_eq(&tc->expect[i], &tc->actual[i]))
        break;
    if (i == tc->expect_cnt) {
      printf(": OK\n");
      return;
    }
  }
  /* Test failed.  Log formatted table of expected vs actual args. */
  printf(": FAIL\n");
  tmp = copt_dbg_dump();
  logf("%s", tmp ? tmp : "");
  free(tmp);
  logf("%s:%d: ", __FILE__, test_line);
  logf_args(tc->argc, tc->argv, 0);
  logf("\n");
  for (i = 0; i < tc->argc && !strcmp(tc->argv[i], tc->argv_copy[i]); i++)
    continue;
  if (i != tc->argc) {
    logf("(reordered to ");
    logf_args(tc->argc, tc->argv_copy, 0);
    logf(")\n");
  }
  if (tc->expect_cnt != tc->actual_cnt)
    logf("  expected %lu args, found %lu\n",
         (long) tc->expect_cnt, (long) tc->actual_cnt);
  testcase_dump(tc);
  failed_test_cnt++;
}

static void
test_end(struct testcase *tc, int reorder)
{
  struct copt opt;
  size_t i;
  for (i = tc->argc; i < sizeof tc->argv / sizeof *tc->argv; i++)
    tc->argv[i] = (char *) "@@@@@@@ OUT-OF-BOUNDS @@@@@@@";
  memcpy(tc->argv_copy, tc->argv, sizeof tc->argv);

  opt = copt_init((int) tc->argc, tc->argv_copy, reorder);
  while (copt_next(&opt)) {
    if (copt_opt(&opt, "x")) {
      actual_opt(tc, "x");
    } else if (copt_opt(&opt, "y")) {
      actual_opt(tc, "y");
    } else if (copt_opt(&opt, "z")) {
      actual_opt(tc, "z");
    } else if (copt_opt(&opt, "longopt")) {
      actual_opt(tc, "longopt");
    } else if (copt_opt(&opt, "m|multiple-opts")) {
      actual_opt(tc, "m|multiple-opts");
    } else if (copt_opt(&opt, "s")) {
      actual_opt(tc, "s");
      actual_optarg(tc, copt_arg(&opt));
    } else if (copt_opt(&opt, "long-with-arg")) {
      actual_opt(tc, "long-with-arg");
      actual_optarg(tc, copt_arg(&opt));
    } else if (copt_opt(&opt, "a|multiple-with-arg")) {
      actual_opt(tc, "a|multiple-with-arg");
      actual_optarg(tc, copt_arg(&opt));
    } else if (copt_opt(&opt, "o|optional-arg=")) {
      actual_opt(tc, "o|optional-arg=");
      actual_optarg(tc, copt_arg(&opt));
    } else {
      actual_badopt(tc, copt_curopt(&opt));
    }
  }
  for (i = copt_idx(&opt); i < tc->argc; i++)
    actual_arg(tc, tc->argv_copy[i]);

  test_verify(tc);
  for (i = 0; i < tc->argc; i++)
    free(tc->argv[i]);
  for (i = 0; i < tc->expect_cnt; i++)
    free((void *) tc->expect[i].val); /* discard const */
  for (i = 0; i < tc->actual_cnt; i++)
    free((void *) tc->actual[i].val); /* discard const */
}

static void
run_copt_tests(int reorder)
{
  /* Permutations of these args/opts/optargs are included between
     TEST_BEGIN()/TEST_END() pairs. */
  static const char *const pre_args[][3] = {
    {0}, {"pre-foo"}, {"pre-foo", "pre-bar"}, {"-"},
    {"pre-asdf", "-"},  {"-", "pre-fdsa"}
  };
  static const char *const post_args[][3] = {
    {0}, {"post-foo"}, {"post-foo", "post-bar"}, {"-"},
    {"post-asdf", "-"}, {"-", "post-fdsa"}
  };
  static const char *const opts[][3] = { /* input opts */
    {0}, {"-x"}, {"-x","-y"}, {"-xy"}, {"-xyzzy"}, {"-m"},
    {"--multiple-opts"}, {"-xm"}, {"-mx"}, {"-s","sarg"},
    {"-ssarg"}, {"-sx"}, {"-xssarg"}, {"-xs","sarg"}
  };
  static const struct arg exp[][6] = { /* expected output */
    {{ARGTYPE_NULL}}, {{ARGTYPE_OPT,"x"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"y"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"y"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"y"}, {ARGTYPE_OPT,"z"},
     {ARGTYPE_OPT,"z"}, {ARGTYPE_OPT,"y"}},
    {{ARGTYPE_OPT,"m|multiple-opts"}},
    {{ARGTYPE_OPT,"m|multiple-opts"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"m|multiple-opts"}},
    {{ARGTYPE_OPT,"m|multiple-opts"}, {ARGTYPE_OPT,"x"}},
    {{ARGTYPE_OPT,"s"}, {ARGTYPE_OPTARG,"sarg"}},
    {{ARGTYPE_OPT,"s"}, {ARGTYPE_OPTARG,"sarg"}},
    {{ARGTYPE_OPT,"s"}, {ARGTYPE_OPTARG,"x"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"s"},
     {ARGTYPE_OPTARG,"sarg"}},
    {{ARGTYPE_OPT,"x"}, {ARGTYPE_OPT,"s"},
     {ARGTYPE_OPTARG,"sarg"}}
  };

#define TEST_BEGIN(use_pre_args, use_post_args, test_args) {            \
    char **test_args_ = (test_args);                                    \
    const size_t pre_cnt_ = !(use_pre_args) ? 1 :                       \
                            sizeof pre_args / sizeof *pre_args;         \
    const size_t post_cnt_ = !(use_post_args) ? 1 :                     \
                             sizeof post_args / sizeof *post_args;      \
    size_t i_, j_, k_, pre_, post_;                                     \
                                                                        \
    for (pre_ = 0; pre_ < pre_cnt_; pre_++) {                           \
      for (post_ = 0; post_ < post_cnt_; post_++) {                     \
        assert(sizeof opts / sizeof *opts == sizeof exp / sizeof *exp); \
        for (j_ = 0; j_ < sizeof opts / sizeof *opts; j_++) {           \
          for (k_ = 0; k_ < sizeof opts / sizeof *opts; k_++) {         \
            int argbrk_done_ = 0, test_args_done_ = 0;                  \
            (void) test_args_done_; /* set-but-not-used warning */      \
            test_begin(&tc, NULL);                                      \
            test_addargs(&tc, (char **) opts[j_]);                      \
            test_addargs(&tc, (char **) pre_args[pre_]);                \
            test_addargs(&tc, (char **) opts[k_]);                      \
            test_addargs(&tc, test_args_);                              \
            test_addargs(&tc, (char **) post_args[post_]);              \
            for (i_ = 0; exp[j_][i_].type; i_++)                        \
              expect(&tc, exp[j_][i_].type, exp[j_][i_].val);           \
            if (!reorder && pre_args[pre_][0]) {                        \
              for (i_ = 0; pre_args[pre_][i_]; i_++)                    \
                expect_arg(&tc, pre_args[pre_][i_]);                    \
              for (i_ = 0; opts[k_][i_]; i_++)                          \
                expect_arg(&tc, opts[k_][i_]);                          \
              for (i_ = 0; test_args_[i_]; i_++)                        \
                expect_arg(&tc, test_args_[i_]);                        \
              test_args_done_ = 1;                                      \
            } else {                                                    \
              for (i_ = 0; exp[k_][i_].type; i_++)                      \
                expect(&tc, exp[k_][i_].type, exp[k_][i_].val);         \
              ((void) 0)

#define TEST_ARGBRK()                                                   \
            }                                                           \
            if (reorder) {                                              \
              for (i_ = 0; pre_args[pre_][i_]; i_++)                    \
                expect_arg(&tc, pre_args[pre_][i_]);                    \
              argbrk_done_ = 1;                                         \
            }                                                           \
            if (!test_args_done_) { ((void) 0)

#define TEST_END()                                                      \
            }                                                           \
            if (reorder && !argbrk_done_)                               \
              for (i_ = 0; pre_args[pre_][i_]; i_++)                    \
                expect_arg(&tc, pre_args[pre_][i_]);                    \
            for (i_ = 0; post_args[post_][i_]; i_++)                    \
              expect_arg(&tc, post_args[post_][i_]);                    \
            test_end(&tc, reorder);                                     \
          }                                                             \
        }                                                               \
      }                                                                 \
    }                                                                   \
    free(test_args_);                                                   \
  }

  struct testcase tc;
  size_t i;

  /* test basic functionality */
  test_begin(&tc, NULL);
  assert(tc.actual_cnt == 0);
  test_end(&tc, reorder);

  test_begin(&tc, "arg1", NULL);
  expect_arg(&tc, "arg1");
  test_end(&tc, reorder);

  test_begin(&tc, "arg1", "arg2", NULL);
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_end(&tc, reorder);

  TEST_BEGIN(1, 1, mkargv("-x", NULL));
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-y", NULL));
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "-y", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  TEST_END();

  /* grouped short opts */
  TEST_BEGIN(1, 1, mkargv("-xyz", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyzzy", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--longopt", "-xyzzy", NULL));
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyzzy", "--longopt", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-z", "-xyzzy", NULL));
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyzzy", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  TEST_END();

  /* verify option parsing stops when encountering non-options */
  test_begin(&tc, "-x", "nonopt", "-y", NULL);
  expect_opt(&tc, "x");
  if (reorder) {
    expect_opt(&tc, "y");
    expect_arg(&tc, "nonopt");
  } else {
    expect_arg(&tc, "nonopt");
    expect_arg(&tc, "-y");
  }
  test_end(&tc, reorder);

  test_begin(&tc, "-y", "--", "-x", NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "-x");
  test_end(&tc, reorder);

  test_begin(&tc, "foo", "-y", "--", "-x", NULL);
  if (reorder) {
    expect_opt(&tc, "y");
    expect_arg(&tc, "foo");
  } else {
    expect_arg(&tc, "foo");
    expect_arg(&tc, "-y");
    expect_arg(&tc, "--");
  }
  expect_arg(&tc, "-x");
  test_end(&tc, reorder);

  test_begin(&tc, "-z", "--", "-y", "nonopt", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt");
  test_end(&tc, reorder);

  test_begin(&tc, "-z", "--", "-y", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_end(&tc, reorder);

  test_begin(&tc, "-x", "-", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_end(&tc, reorder);

  test_begin(&tc, "foo", "-z", "--", "-y", NULL);
  if (reorder) {
    expect_opt(&tc, "z");
    expect_arg(&tc, "foo");
  } else {
    expect_arg(&tc, "foo");
    expect_arg(&tc, "-z");
    expect_arg(&tc, "--");
  }
  expect_arg(&tc, "-y");
  test_end(&tc, reorder);

  test_begin(&tc, "-", "-z", "--", "-y", NULL);
  if (reorder) {
    expect_opt(&tc, "z");
    expect_arg(&tc, "-");
  } else {
    expect_arg(&tc, "-");
    expect_arg(&tc, "-z");
    expect_arg(&tc, "--");
  }
  expect_arg(&tc, "-y");
  test_end(&tc, reorder);

  /* unknown short and long options */
  for (i = 0; i < 2; i++) {
    static const char *unknown[2] = { "-q", "--unknown-opt" };
    TEST_BEGIN(1, 1, mkargv(unknown[i], NULL));
    expect_badopt(&tc, unknown[i]);
    TEST_END();

    TEST_BEGIN(1, 1, mkargv("-x", unknown[i], NULL));
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    TEST_END();

    TEST_BEGIN(1, 1, mkargv("-x", unknown[i], "-y", NULL));
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "y");
    TEST_END();
  }

  /* unknown options grouped with known options */
  TEST_BEGIN(1, 1, mkargv("-qx", NULL));
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-qs", "sarg", NULL));
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_END();

  TEST_BEGIN(1, 0, mkargv("-qs", NULL));
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-yqx", NULL));
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-zqs", "sarg", NULL));
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_END();

  TEST_BEGIN(1, 0, mkargv("-xqs", NULL));
  expect_opt(&tc, "x");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-zxyqx", NULL));
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xy", "-zqs", "sarg", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xy", "-zqs", "sarg", "arg1", "arg2", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_ARGBRK();
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  TEST_END();

  TEST_BEGIN(1, 0, mkargv("--longopt", "-xyzqs", NULL));
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--longopt", "-xyzqs", "arg1", "arg2", NULL));
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "arg1");
  TEST_ARGBRK();
  expect_arg(&tc, "arg2");
  TEST_END();

  /* short opts with args */
  TEST_BEGIN(1, 1, mkargv("-s", "sarg", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "sarg", "-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 0, mkargv("-s", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-ssarg", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-ssarg", "-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 0, mkargv("-s", "sarg", "--long-with-arg", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "sarg", "--long-with-arg", "-m", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "m|multiple-opts");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "sarg", "--long-with-arg=-m", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "-m");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xys", "sarg", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyssarg", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xys", "sarg", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  TEST_END();

  /* don't confuse optargs with actual options */
  TEST_BEGIN(1, 1, mkargv("-sx", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-sxy", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xy");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "-x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-syz", "-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "-syz", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  TEST_END();

  /* don't confuse optargs with actual options when grouped */
  TEST_BEGIN(1, 1, mkargv("-xsx", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyzsxs", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xs");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xs", "-y", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-xyzs", "-xy", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--longopt", "-x", "-syz", "-z", NULL));
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "--longopt", "-syz", "-z", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "--longopt", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "--longopt", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-slongopt", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s--longopt", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "--longopt", "-s", "--longopt", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "--longopt", "-s--longopt", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "--longopt", "-slongopt", NULL));
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  TEST_END();

  /* long opts with args */
  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "optarg", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "optarg", "-x", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg=optarg", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg=--", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  TEST_END();

  /* missing arg for short option */
  TEST_BEGIN(1, 0, mkargv("-s", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "-x", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "-q", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "--", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-s", "--", "-s", "foo", NULL));
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  TEST_ARGBRK();
  expect_arg(&tc, "-s");
  expect_arg(&tc, "foo");
  TEST_END();

  test_begin(&tc, "-s", "--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_end(&tc, reorder);

  /* missing arg for long option */
  TEST_BEGIN(1, 0, mkargv("--long-with-arg", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "-x", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "-q", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "--", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "--", "-x", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  TEST_ARGBRK();
  expect_arg(&tc, "-x");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("--long-with-arg", "--longopt", NULL));
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  TEST_END();

  /* options where arg itself is optional */
  TEST_BEGIN(1, 1, mkargv("-ooarg", NULL));
  expect_opt(&tc, "o|optional-arg=");
  expect_optarg(&tc, "oarg");
  TEST_END();

  for (i = 0; i < 2; i++) {
    static const char *baseopt[] = {"-o", "--optional-arg"};
    char optbuf[128];
    assert(strlen(baseopt[i]) + strlen("=oarg") + 1 < sizeof optbuf);

    snprintf(optbuf, sizeof optbuf, "%s", baseopt[i]);
    TEST_BEGIN(1, 1, mkargv(optbuf, NULL));
    expect_opt(&tc, "o|optional-arg=");
    expect_optarg(&tc, NULL);
    TEST_END();

    snprintf(optbuf, sizeof optbuf, "%s=", baseopt[i]);
    TEST_BEGIN(1, 1, mkargv(optbuf, NULL));
    expect_opt(&tc, "o|optional-arg=");
    expect_optarg(&tc, "");
    TEST_END();

    snprintf(optbuf, sizeof optbuf, "%s=oarg", baseopt[i]);
    TEST_BEGIN(1, 1, mkargv(optbuf, NULL));
    expect_opt(&tc, "o|optional-arg=");
    expect_optarg(&tc, "oarg");
    TEST_END();

    snprintf(optbuf, sizeof optbuf, "%s=", baseopt[i]);
    TEST_BEGIN(1, 1, mkargv(optbuf, "nonopt", NULL));
    expect_opt(&tc, "o|optional-arg=");
    expect_optarg(&tc, "");
    TEST_ARGBRK();
    expect_arg(&tc, "nonopt");
    TEST_END();

    snprintf(optbuf, sizeof optbuf, "%s", baseopt[i]);
    TEST_BEGIN(1, 1, mkargv(optbuf, "nonopt", NULL));
    expect_opt(&tc, "o|optional-arg=");
    expect_optarg(&tc, NULL);
    TEST_ARGBRK();
    expect_arg(&tc, "nonopt");
    TEST_END();
  }

  /* verify "-" is treated as a non-option or option argument */
  TEST_BEGIN(1, 1, mkargv("-", NULL));
  TEST_ARGBRK();
  expect_arg(&tc, "-");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x", "-", NULL));
  expect_opt(&tc, "x");
  TEST_ARGBRK();
  expect_arg(&tc, "-");
  TEST_END();

  TEST_BEGIN(1, 1, mkargv("-x-", NULL));
  expect_opt(&tc, "x");
  expect_badopt(&tc, "--");
  TEST_END();

  for (i = 0; i < 5; i++) {
    if (i == 0) test_begin(&tc, "-s", "-", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-s", "-", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-s", "-", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-s", "-", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-s", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-s-", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-s-", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-s-", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-s-", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-s-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-s", "-", "-", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-s", "-", "-", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-s", "-", "-", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-s", "-", "-", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-s", "-", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "-");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-s" "-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-s" "-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-s" "-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-s" "-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-s" "-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-s-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-s-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-s-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-s-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-s-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-xys", "-", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-xys", "-", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-xys", "-", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-xys", "-", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-xys", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "x");
    expect_opt(&tc, "y");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-xzs-", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-xzs-", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-xzs-", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-xzs-", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-xzs-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "x");
    expect_opt(&tc, "z");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-yys", "-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-yys", "-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-yys", "-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-yys", "-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-yys", "-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "y");
    expect_opt(&tc, "y");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_end(&tc, reorder);

    if (i == 0) test_begin(&tc, "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 1) test_begin(&tc, "-xy", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 2) test_begin(&tc, "-s", "sarg", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 3) test_begin(&tc, "--long-with-arg", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 4) test_begin(&tc, "--long-with-arg", "arg", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "z");
    expect_opt(&tc, "y");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_end(&tc, reorder);
  }
}

int
main(void)
{
  size_t i;
  run_copt_tests(0);
  run_copt_tests(1);

  printf("----\n");
  if (failed_test_cnt == 0)
    printf("Passed all %lu tests\n", (long) total_test_cnt);
  else {
    for (i = 0; i < failed_test_cnt; i++)
      printf("\n%s\n", fail_info[i]);
    printf("FAILED %lu of %lu tests\n",
      (long) failed_test_cnt, (long) total_test_cnt);
  }
  if (fail_info)
    for (i = 0; i < sizeof fail_info / sizeof *fail_info; i++)
      free(fail_info[i]);
  free(fail_info);
  return !!failed_test_cnt;
}
