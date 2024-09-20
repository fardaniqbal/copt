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
static const char *test_file;
static int test_line;

/* Quote STR in static circular buffer and return pointer to it. */
const char *
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
};

static void
testcase_init(struct testcase *tc)
{
  memset(tc, 0, sizeof *tc);
}

static void
testcase_destroy(struct testcase *tc)
{
  size_t i;
  for (i = 0; i < tc->expect_cnt; i++)
    free(tc->expect[i].val);
  for (i = 0; i < tc->actual_cnt; i++)
    free(tc->actual[i].val);
}

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
test_begin(struct testcase *tc, int reorder, ...)
{
  struct copt opt;
  size_t i;
  va_list ap;
  testcase_init(tc);
  tc->argv[tc->argc++] = "testprog";
  va_start(ap, reorder);
  do {
    assert(tc->argc < sizeof tc->argv / sizeof *tc->argv);
    tc->argv[tc->argc] = va_arg(ap, char *);
  } while (tc->argv[tc->argc++] != NULL);
  va_end(ap);
  assert(tc->argc > 0);
  tc->argc--;
  assert(tc->argv[tc->argc] == NULL);

  opt = copt_init((int) tc->argc, tc->argv, reorder);
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
    actual_arg(tc, tc->argv[i]);
}

#define test_begin (test_file = __FILE__, test_line = __LINE__, test_begin)

static void
print_args(size_t argc, char **argv, int max_width)
{
  static char buf[1024];
  size_t i;
  buf[0] = '\0';
  for (i = 0; i < argc; i++)
    strcat(strcat(strcat(buf, "'"), argv[i]), "' ");
  if (max_width >= 0 && strlen(buf) > (size_t) max_width)
    strcpy(buf + max_width - 3, "...");
  printf("%-*s", max_width >= 0 ? max_width : 0, buf);
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
      testcase_destroy(tc);
      return;
    }
  }
  /* Test failed.  Print formatted table of expected vs actual args. */
  printf(": FAIL\n\n");
  printf("%s:%d: ", test_file, test_line);
  print_args(tc->argc, tc->argv, -1);
  printf("\n");
  if (tc->expect_cnt != tc->actual_cnt)
    printf("  expected %lu args, found %lu\n",
           (long) tc->expect_cnt, (long) tc->actual_cnt);
  testcase_dump(tc);
  printf("\n");
  testcase_destroy(tc);
}

static void
run_copt_tests(int reorder)
{
  size_t i;
  struct testcase tc;

  if (reorder) {
    printf("TODO: write tests for arg reordering enabled\n");
    return;
  }

  /* basics */
  test_begin(&tc, reorder, NULL);
  assert(tc.actual_cnt == 0);
  test_verify(&tc);

  test_begin(&tc, reorder, "arg1", NULL);
  expect_arg(&tc, "arg1");
  test_verify(&tc);

  test_begin(&tc, reorder, "arg1", "arg2", NULL);
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", NULL);
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-y", NULL);
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "-y", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  test_verify(&tc);

  /* grouped short opts */
  test_begin(&tc, reorder, "-xyz", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzzy", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzzy", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "--longopt", "-xyzzy", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzzy", "--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-z", "-xyzzy", NULL);
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzzy", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  test_verify(&tc);

  /* verify option parsing stops when encountering non-options */
  test_begin(&tc, reorder, "-x", "nonopt", "-y", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "nonopt");
  expect_arg(&tc, "-y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-y", "--", "-x", NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "-x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-z", "--", "-y", "nonopt", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-z", "--", "-y", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "-", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  /* unknown short and long options */
  for (i = 0; i < 2; i++) {
    static const char *unknown[2] = { "-q", "--unknown-opt" };
    test_begin(&tc, reorder, unknown[i], NULL);
    expect_badopt(&tc, unknown[i]);
    test_verify(&tc);

    test_begin(&tc, reorder, unknown[i], "nonopt", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    test_begin(&tc, reorder, unknown[i], "nonopt1", "nonopt2", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_verify(&tc);

    test_begin(&tc, reorder, "-x", unknown[i], NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    test_verify(&tc);

    test_begin(&tc, reorder, unknown[i], "-x", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    test_verify(&tc);

    test_begin(&tc, reorder, "-x", unknown[i], "-y", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "y");
    test_verify(&tc);

    test_begin(&tc, reorder, "-x", unknown[i], "nonopt", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    test_begin(&tc, reorder, unknown[i], "-x", "nonopt", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    test_begin(&tc, reorder, "-x", unknown[i], "-y", "nonopt", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "y");
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    test_begin(&tc, reorder, "-x", unknown[i], "nonopt1", "nonopt2", NULL);
    expect_opt(&tc, "x");
    expect_badopt(&tc, unknown[i]);
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_verify(&tc);

    test_begin(&tc, reorder, unknown[i], "-x", "nonopt1", "nonopt2", NULL);
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_verify(&tc);

    test_begin(&tc, reorder, "-y", unknown[i], "-x", "nonopt1", "nonopt2", NULL);
    expect_opt(&tc, "y");
    expect_badopt(&tc, unknown[i]);
    expect_opt(&tc, "x");
    expect_arg(&tc, "nonopt1");
    expect_arg(&tc, "nonopt2");
    test_verify(&tc);
  }

  /* unknown options grouped with known options */
  test_begin(&tc, reorder, "-qx", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-qs", "sarg", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-qs", NULL);
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "-yqx", NULL);
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-zqs", "sarg", NULL);
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xqs", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "-zxyqx", NULL);
  expect_opt(&tc, "z");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xy", "-zqs", "sarg", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xy", "-zqs", "sarg", "arg1", "arg2", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_verify(&tc);

  test_begin(&tc, reorder, "--longopt", "-xyzqs", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "--longopt", "-xyzqs", "arg1", "arg2", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_badopt(&tc, "-q");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_verify(&tc);

  /* short opts with args */
  test_begin(&tc, reorder, "-s", "sarg", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "sarg", "nonopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "sarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "sarg", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xys", "sarg", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xys", "sarg", "-z", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-ssarg", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-ssarg", "nonopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-ssarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-ssarg", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyssarg", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyssarg", "-z", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "sarg");
  expect_opt(&tc, "z");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  /* don't confuse optargs with actual options */
  test_begin(&tc, reorder, "-sx", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-sxy", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xy");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "-x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-sx", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "-syz", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "-syz", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_verify(&tc);

  /* don't confuse optargs with actual options when grouped */
  test_begin(&tc, reorder, "-xsx", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xsx", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "x");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzsxs", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xs");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzsxs", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "xs");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xs", "-y", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xs", "-y", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzs", "-xy", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_begin(&tc, reorder, "-xyzs", "-xy", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "--longopt", "-x", "-syz", "-z", NULL);
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "x");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "--longopt", "-syz", "-z", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "--longopt", "-syz", "-z", "foo", "bar", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "yz");
  expect_opt(&tc, "z");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "--longopt", "foo", "bar", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-slongopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s--longopt", "foo", "bar", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  expect_arg(&tc, "foo");
  expect_arg(&tc, "bar");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "--longopt", "-s", "--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "--longopt", "-s--longopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "--longopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "--longopt", "-slongopt", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "longopt");
  expect_opt(&tc, "s");
  expect_optarg(&tc, "longopt");
  test_verify(&tc);

  /* long opts with args */
  test_begin(&tc, reorder, "--long-with-arg", "optarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "optarg", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "optarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "optarg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=optarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=optarg", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=optarg", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=optarg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "optarg");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=--", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=--", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=--", "nonopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg=--", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, "--");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  /* missing arg for short option */
  test_begin(&tc, reorder, "-s", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "-x", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "-q", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "--", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "--", "notoptarg", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "notoptarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "-s", "--longopt", NULL);
  expect_opt(&tc, "s");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_verify(&tc);

  /* missing arg for long option */
  test_begin(&tc, reorder, "--long-with-arg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "-x", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "-q", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_badopt(&tc, "-q");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "--", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "--", "notoptarg", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_arg(&tc, "notoptarg");
  test_verify(&tc);

  test_begin(&tc, reorder, "--long-with-arg", "--longopt", NULL);
  expect_opt(&tc, "long-with-arg");
  expect_optarg(&tc, NULL);
  expect_opt(&tc, "longopt");
  test_verify(&tc);

  /* verify "-" is treated as a non-option or option argument */
  test_begin(&tc, reorder, "-", NULL);
  expect_arg(&tc, "-");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "-", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x", "-", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x-", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "--");
  test_verify(&tc);

  test_begin(&tc, reorder, "-x-", "nonopt", NULL);
  expect_opt(&tc, "x");
  expect_badopt(&tc, "--");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  for (i = 0; i < 5; i++) {
    if (i == 0) test_begin(&tc, reorder, "-s", "-", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-s", "-", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-s", "-", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-s", "-", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-s", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-s-", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-s-", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-s-", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-s-", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-s-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-s", "-", "-", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-s", "-", "-", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-s", "-", "-", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-s", "-", "-", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-s", "-", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "-");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-s" "-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-s" "-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-s" "-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-s" "-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-s" "-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-s-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-s-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-s-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-s-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-s-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-xys", "-", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-xys", "-", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-xys", "-", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-xys", "-", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-xys", "-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "x");
    expect_opt(&tc, "y");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-xzs-", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-xzs-", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-xzs-", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-xzs-", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-xzs-", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "x");
    expect_opt(&tc, "z");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-yys", "-", "nonopt", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-yys", "-", "nonopt", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-yys", "-", "nonopt", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-yys", "-", "nonopt", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-yys", "-", "nonopt", NULL);
    if (i == 1) expect_opt(&tc, "x"), expect_opt(&tc, "y");
    if (i == 2) expect_opt(&tc, "s"), expect_optarg(&tc, "sarg");
    if (i == 3) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, NULL);
    if (i == 4) expect_opt(&tc, "long-with-arg"), expect_optarg(&tc, "arg");
    expect_opt(&tc, "y");
    expect_opt(&tc, "y");
    expect_opt(&tc, "s");
    expect_optarg(&tc, "-");
    expect_arg(&tc, "nonopt");
    test_verify(&tc);

    if (i == 0) test_begin(&tc, reorder, "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 1) test_begin(&tc, reorder, "-xy", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 2) test_begin(&tc, reorder, "-s", "sarg", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 3) test_begin(&tc, reorder, "--long-with-arg", "-zys-", "nonopt1", "nonopt2", NULL);
    if (i == 4) test_begin(&tc, reorder, "--long-with-arg", "arg", "-zys-", "nonopt1", "nonopt2", NULL);
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
    test_verify(&tc);
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
