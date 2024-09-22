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
static size_t passed_test_cnt;
static int test_line;

/* Quote STR in static circular buffer and return pointer to it. */
static const char *
quotestr(const char *str)
{
  static char buf[4096];
  static size_t i;
  size_t len = str ? strlen(str) + 3 : strlen("(null)") + 1; 
  size_t start = i + len <= sizeof buf ? i : 0;
  if (!str)
    snprintf(buf+start, sizeof buf - start - 1, "(null)");
  else
    snprintf(buf+start, sizeof buf - start - 1, "'%s'", str);
  i = start + strlen(buf+start) + 1;
  return buf+start;
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
  ARGTYPE_OPT    = 1,
  ARGTYPE_OPTARG = 2,
  ARGTYPE_ARG    = 3,
  ARGTYPE_BADOPT = 4
};

static const char *
argtype_str(enum argtype type)
{
  switch (type) {
    case ARGTYPE_OPT:     return "OPT";
    case ARGTYPE_OPTARG:  return "OPTARG";
    case ARGTYPE_ARG:     return "ARG";
    case ARGTYPE_BADOPT:  return "BADOPT";
    default: printf("bad argtype %d\n", type); exit(1);
  }
}

struct arg {
  enum argtype type;
  char *val;
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

/* Print formatted table of expected vs actual args. */
static void
testcase_dump(const struct testcase *tc)
{
  const int val_width = 27, type_width = 7;
  size_t i;
  printf("%-*s | %s\n", val_width+type_width+1, "EXPECTED", "ACTUAL");
  for (i = 0; i < (size_t) (val_width+type_width+1); i++) printf("-");
  printf(" | ");
  for (i = 0; i < (size_t) (val_width+type_width+1); i++) printf("-");
  printf("\n");

  for (i = 0; i < tc->expect_cnt || i < tc->actual_cnt; i++) {
    const char *ev = NULL, *et = "";
    const char *av = NULL, *at = "";
    if (i < tc->expect_cnt)
      ev = tc->expect[i].val, et = argtype_str(tc->expect[i].type);
    if (i < tc->actual_cnt)
      av = tc->actual[i].val, at = argtype_str(tc->actual[i].type);
    printf("%-*s %-*s | ", val_width, quotestr(ev), type_width, et);
    printf("%-*s %-*s\n",  val_width, quotestr(av), type_width, at);
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
  tc->argv[tc->argc++] = my_strdup("testprog");
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

static void
print_args(size_t argc, char **argv, size_t max_width)
{
  static char buf[1024];
  size_t i;
  buf[0] = '\0';
  for (i = 0; i < argc; i++)
    strcat(strcat(strcat(buf, "'"), argv[i]), "' ");
  if (max_width > 0 && strlen(buf) >= max_width)
    strcpy(buf + max_width - 4, "... ");
  printf("%-*s", max_width > 0 ? (int) max_width : 0, buf);
}

static void
test_verify(struct testcase *tc)
{
  size_t i;
  total_test_cnt++;
  print_args(tc->argc, tc->argv, 70);

  if (tc->expect_cnt == tc->actual_cnt) {
    /* Check if actual args differ from expected args. */
    for (i = 0; i < tc->expect_cnt; i++)
      if (!arg_eq(&tc->expect[i], &tc->actual[i]))
        break;
    if (i == tc->expect_cnt) {
      printf(": OK\n");
      passed_test_cnt++;
      return;
    }
  }
  /* Test failed.  Print formatted table of expected vs actual args. */
  printf(": FAIL\n\n%s:%d: ", __FILE__, test_line);
  print_args(tc->argc, tc->argv, 0);
  printf("\n");
  for (i = 0; i < tc->argc && !strcmp(tc->argv[i], tc->argv_copy[i]); i++)
    continue;
  if (i != tc->argc) {
    printf("(reordered to ");
    print_args(tc->argc, tc->argv_copy, 0);
    printf(")\n");
  }
  copt_dbg_dump();
  if (tc->expect_cnt != tc->actual_cnt)
    printf("  expected %lu args, found %lu\n",
           (long) tc->expect_cnt, (long) tc->actual_cnt);
  testcase_dump(tc);
  printf("\n");
}

static void
test_run(struct testcase *tc, int reorder)
{
  struct copt opt;
  size_t i;
  for (i = tc->argc; i < sizeof tc->argv / sizeof *tc->argv; i++)
    tc->argv[i] = (char *) "@@@@@@@ OUT-OF-BOUNDS @@@@@@@";
  memcpy(tc->argv_copy, tc->argv, sizeof tc->argv);

  opt = copt_init((int) tc->argc, tc->argv_copy, reorder);
  while (!copt_done(&opt)) {
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
    free(tc->expect[i].val);
  for (i = 0; i < tc->actual_cnt; i++)
    free(tc->actual[i].val);
}

static void
run_copt_tests(int reorder)
{
  size_t i, j;
  struct testcase tc;
  (void) j; /* TODO: deleteme */

  /* test basic functionality */
  test_begin(&tc, NULL);
  assert(tc.actual_cnt == 0);
  test_run(&tc, reorder);

  test_begin(&tc, "arg1", NULL);
  expect_arg(&tc, "arg1");
  test_run(&tc, reorder);

  test_begin(&tc, "arg1", "arg2", NULL);
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_run(&tc, reorder);

  test_begin(&tc, "arg1", "-x", "arg2", NULL);
  if (reorder) expect_opt(&tc, "x");
  expect_arg(&tc, "arg1");
  if (!reorder) expect_arg(&tc, "-x");
  expect_arg(&tc, "arg2");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", NULL);
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-y", NULL);
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "-y", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  /* grouped short opts */
  test_begin(&tc, "-xyz", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzzy", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzzy", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "nonopt", "-xyzzy", NULL);
  if (reorder) {
    expect_opt(&tc, "x");
    expect_opt(&tc, "y");
    expect_opt(&tc, "z");
    expect_opt(&tc, "z");
    expect_opt(&tc, "y");
    expect_arg(&tc, "nonopt");
  } else {
    expect_arg(&tc, "nonopt");
    expect_arg(&tc, "-xyzzy");
  }
  test_run(&tc, reorder);

  test_begin(&tc, "--longopt", "-xyzzy", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzzy", "--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-z", "-xyzzy", NULL);
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzzy", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  test_run(&tc, reorder);

  /* verify option parsing stops when encountering non-options */
  test_begin(&tc, "-x", "nonopt", "-y", NULL);
  expect_opt(&tc, "x");
  if (!reorder) {
    expect_arg(&tc, "nonopt");
    expect_arg(&tc, "-y");
  } else {
    expect_opt(&tc, "y");
    expect_arg(&tc, "nonopt");
  }
  test_run(&tc, reorder);

  test_begin(&tc, "-y", "--", "-x", NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "-x");
  test_run(&tc, reorder);

  test_begin(&tc, "-z", "--", "-y", "nonopt", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-z", "--", "-y", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "-", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  /* unknown short and long options */
  for (i = 0; i < 2; i++) {
    static const char *unknown[2] = { "-q", "--unknown-opt" };
    test_begin(&tc, unknown[i], NULL);
    expect_badopt(&tc, unknown[i]);
    test_run(&tc, reorder);

    test_begin(&tc, unknown[i], "nonopt", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt");
    test_run(&tc, reorder);

    test_begin(&tc, unknown[i], "nonopt1", "nonopt2", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_run(&tc, reorder);

    test_begin(&tc, "nonopt", unknown[i], NULL);
    if (reorder) expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt");
    if (!reorder) expect_arg(&tc, unknown[i]);
    test_run(&tc, reorder);

    test_begin(&tc, "nonopt1", "nonopt2", unknown[i], NULL);
    if (reorder) expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    if (!reorder) expect_arg(&tc, unknown[i]);
    test_run(&tc, reorder);

    test_begin(&tc, "-x", unknown[i], NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    test_run(&tc, reorder);

    test_begin(&tc, unknown[i], "-x", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    test_run(&tc, reorder);

    test_begin(&tc, "-x", unknown[i], "-y", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "y");
    test_run(&tc, reorder);

    test_begin(&tc, "-x", unknown[i], "nonopt", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt");
    test_run(&tc, reorder);

    test_begin(&tc, unknown[i], "-x", "nonopt", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt");
    test_run(&tc, reorder);

    test_begin(&tc, "-x", unknown[i], "-y", "nonopt", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "y");
    expect_arg(&tc, "nonopt");
    test_run(&tc, reorder);

    test_begin(&tc, "-x", unknown[i], "nonopt1", "nonopt2", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_run(&tc, reorder);

    test_begin(&tc, unknown[i], "-x", "nonopt1", "nonopt2", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_run(&tc, reorder);

    test_begin(&tc, "-y", unknown[i], "-x", "nonopt1", "nonopt2", NULL);
    expect_opt(&tc, "y");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_run(&tc, reorder);
  }

  /* unknown options grouped with known options */
  test_begin(&tc, "-qx", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-qs", "sarg", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_run(&tc, reorder);

  test_begin(&tc, "-qs", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "-yqx", NULL);
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-zqs", "sarg", NULL);
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_run(&tc, reorder);

  test_begin(&tc, "-xqs", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "-zxyqx", NULL);
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-xy", "-zqs", "sarg", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_run(&tc, reorder);

  test_begin(&tc, "-xy", "-zqs", "sarg", "arg1", "arg2", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_run(&tc, reorder);

  test_begin(&tc, "--longopt", "-xyzqs", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "--longopt", "-xyzqs", "arg1", "arg2", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_run(&tc, reorder);

  /* short opts with args */
  {
    {
#define TEST_BEGIN(use_pre_args, use_post_args, test_args) {            \
    static const char *pre_args[][3] = {                                \
      {0}, {"pre-foo"}, {"pre-foo", "pre-bar"}                          \
    };                                                                  \
    static const char *post_args[][3] = {                               \
      {0}, {"post-foo"}, {"post-foo", "post-bar"}                       \
    };                                                                  \
    char **test_args_ = (test_args);                                    \
    size_t i_, pre_, post_;                                             \
    for (pre_ = 0; pre_ < ((use_pre_args) ? 3 : 1); pre_++) {           \
      for (post_ = 0; post_ < ((use_post_args) ? 3 : 1); post_++) {     \
        test_begin(&tc, NULL);                                          \
        test_addargs(&tc, (char **) pre_args[pre_]);                    \
        test_addargs(&tc, test_args_);                                  \
        test_addargs(&tc, (char **) post_args[post_]);                  \
        if (!reorder && pre_args[pre_][0]) {                            \
          for (i_ = 0; pre_args[pre_][i_]; i_++)                        \
            expect_arg(&tc, pre_args[pre_][i_]);                        \
          for (i_ = 0; test_args_[i_]; i_++)                            \
            expect_arg(&tc, test_args_[i_]);                            \
        } else { ((void) 0)
#define TEST_END()                                                      \
        }                                                               \
        if (reorder)                                                    \
           for (i_ = 0; pre_args[pre_][i_]; i_++)                       \
             expect_arg(&tc, pre_args[pre_][i_]);                       \
        for (i_ = 0; post_args[post_][i_]; i_++)                        \
          expect_arg(&tc, post_args[post_][i_]);                        \
        test_run(&tc, reorder);                                         \
      }                                                                 \
    }                                                                   \
    free(test_args_);                                                   \
  }
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

#if 0 /* TODO: refactor TEST_BEGIN/TEST_END macros to work with non-option
         args in the middle of options. */
      if (reorder) {
        TEST_BEGIN(1, 1, mkargv("-xyssarg", "-z", "foo", "-x", "bar", NULL));
        expect_opt(&tc, "x");
        expect_opt(&tc, "y");
        expect_opt(&tc, "s");
        expect_optarg(&tc, "sarg");
        expect_opt(&tc, "z");
        expect_opt(&tc, "x");
        expect_arg(&tc, "foo");
        expect_arg(&tc, "-x");
        expect_arg(&tc, "bar");
        TEST_END();
      } else {
        TEST_BEGIN(1, 1, mkargv("-xyssarg", "-z", "foo", "-x", "bar", NULL));
        expect_opt(&tc, "x");
        expect_opt(&tc, "y");
        expect_opt(&tc, "s");
        expect_optarg(&tc, "sarg");
        expect_opt(&tc, "z");
        expect_opt(&tc, "x");
        expect_arg(&tc, "foo");
        expect_arg(&tc, "-x");
        expect_arg(&tc, "bar");
        TEST_END();
      }
#else
      test_begin(&tc, "-xyssarg", "-z", "foo", "-x", "bar", NULL);
      expect_opt(&tc, "x");
      expect_opt(&tc, "y");
      expect_opt(&tc, "s");
      expect_optarg(&tc, "sarg");
      expect_opt(&tc, "z");
      if (reorder) expect_opt(&tc, "x");
      expect_arg(&tc, "foo");
      if (!reorder) expect_arg(&tc, "-x");
      expect_arg(&tc, "bar");
      test_run(&tc, reorder);

      test_begin(&tc, "-xys", "sarg", "-z", "foo", "-x", "bar", NULL);
      expect_opt(&tc, "x");
      expect_opt(&tc, "y");
      expect_opt(&tc, "s");
      expect_optarg(&tc, "sarg");
      expect_opt(&tc, "z");
      if (reorder) expect_opt(&tc, "x");
      expect_arg(&tc, "foo");
      if (!reorder) expect_arg(&tc, "-x");
      expect_arg(&tc, "bar");
      test_run(&tc, reorder);
#endif
    }
  }

  /* don't confuse optargs with actual options */
  test_begin(&tc, "-sx", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-sxy", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xy");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-s-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "-x");
  test_run(&tc, reorder);

  test_begin(&tc, "-sx", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  test_begin(&tc, "nonopt1", "-sx", "nonopt2", NULL);
  if (reorder) expect_opt(&tc, "s");
  if (reorder) expect_optarg(&tc, "x");
  expect_arg(&tc, "nonopt1");
  if (!reorder) expect_arg(&tc, "-sx");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  test_begin(&tc, "-syz", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "-syz", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_run(&tc, reorder);

  /* don't confuse optargs with actual options when grouped */
  test_begin(&tc, "-xsx", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-xsx", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzsxs", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xs");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzsxs", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xs");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-xs", "-y", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-xs", "-y", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzs", "-xy", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  test_run(&tc, reorder);

  test_begin(&tc, "-xyzs", "-xy", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "--longopt", "-x", "-syz", "-z", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "--longopt", "-syz", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "--longopt", "-syz", "-z", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--longopt", "foo", "bar", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-slongopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-s--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-s--longopt", "foo", "bar", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "--longopt", "-s", "--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "--longopt", "-s--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "--longopt", "-slongopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  test_run(&tc, reorder);

  /* long opts with args */
  test_begin(&tc, "--long-with-arg", "optarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "optarg", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "optarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "optarg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=optarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=optarg", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=optarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=optarg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=--", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=--", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=--", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg=--", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_run(&tc, reorder);

  /* missing arg for short option */
  test_begin(&tc, "-s", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "-q", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--", "notoptarg", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "notoptarg");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--", "-s", "foo", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "-s");
  expect_arg(&tc, "foo");
  test_run(&tc, reorder);

  test_begin(&tc, "-s", "--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_run(&tc, reorder);

  /* missing arg for long option */
  test_begin(&tc, "--long-with-arg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "-q", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "--", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "--", "notoptarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "notoptarg");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "--", "-x", "notoptarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "-x");
  expect_arg(&tc, "notoptarg");
  test_run(&tc, reorder);

  test_begin(&tc, "--long-with-arg", "--longopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_run(&tc, reorder);

  /* verify "-" is treated as a non-option or option argument */
  test_begin(&tc, "-", NULL);
  expect_arg(&tc, "-");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "-", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  test_run(&tc, reorder);

  test_begin(&tc, "-x", "-", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

  test_begin(&tc, "-x-", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "--");
  test_run(&tc, reorder);

  test_begin(&tc, "-x-", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "--");
  expect_arg(&tc, "nonopt");
  test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);

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
    test_run(&tc, reorder);
  }
}

int
main(void)
{
  run_copt_tests(0);
  run_copt_tests(1);

  printf("----\n");
  if (passed_test_cnt == total_test_cnt)
    printf("Passed all %lu tests\n", (long) total_test_cnt);
  else
    printf("FAILED %lu of %lu tests\n",
      (long) (total_test_cnt - passed_test_cnt), (long) total_test_cnt);
  return passed_test_cnt != total_test_cnt;
}
