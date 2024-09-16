#include "copt.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef NDEBUG
#include <assert.h>

enum argtype
{
  ARGTYPE_OPT = 1,
  ARGTYPE_OPTARG = 2,
  ARGTYPE_ARG = 3,
  ARGTYPE_BADOPT = 4
};

struct arg
{
  enum argtype type;
  const char *val;
};

struct testcase
{
  size_t expect_cnt;
  size_t actual_cnt;
  struct arg expect[64];
  struct arg actual[64];
  size_t argc;
  char *argv[64];
};

static struct testcase
testcase_init(void) {
  struct testcase tc;
  memset(&tc, 0, sizeof tc);
  return tc;
}

static void
actual(struct testcase *tc, enum argtype type, const char *val)
{
  size_t i = tc->actual_cnt++;
  assert(i < sizeof tc->actual / sizeof *tc->actual);
  tc->actual[i].type = type;
  tc->expect[i].val = val;
}

static void
expect(struct testcase *tc, enum argtype type, const char *val)
{
  size_t i = tc->expect_cnt++;
  assert(i < sizeof tc->expect / sizeof *tc->expect);
  tc->expect[i].type = type;
  tc->expect[i].val = val;
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
verifyszeq(size_t expect, size_t actual)
{
  if (expect != actual) {
    printf("FAIL\n");
    printf("  expected : %lu\n", (unsigned long) expect);
    printf("  actual   : %lu\n", (unsigned long) actual);
    exit(1);
  }
}

static void
verifystreq(const char *expect, const char *actual)
{
  if (strcmp(expect, actual)) {
    printf("FAIL\n");
    printf("  expected : '%s'\n", expect);
    printf("  actual   : '%s'\n", actual);
    exit(1);
  }
}

static void
test_setup(struct testcase *tc, int reorder, ...)
{
  struct copt opt;
  size_t i;
  va_list ap;
  *tc = testcase_init();
  tc->argv[tc->argc++] = "testprog";
  va_start(ap, reorder);
  do {
    assert(tc->argc < sizeof tc->argv / sizeof *tc->argv);
    tc->argv[tc->argc] = va_arg(ap, char *);
  } while (tc->argv[tc->argc++] != NULL);
  va_end(ap);

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

static void
test_verify(struct testcase *tc)
{
  size_t i;
  verifyszeq(tc->expect_cnt, tc->actual_cnt);
  for (i = 0; i < tc->expect_cnt; i++)
    /* TODO */;
}

int
main(void)
{
  struct testcase tc;
  test_setup(&tc, 0, NULL);
  verifyszeq(0, tc.actual_cnt);
  test_verify(&tc);

  test_setup(&tc, 0, "arg1", NULL);
  expect_arg(&tc, "arg1");
  test_verify(&tc);

  test_setup(&tc, 0, "arg1", "arg2", NULL);
  expect_arg(&tc, "arg1");
  expect_arg(&tc, "arg2");
  test_verify(&tc);

  test_setup(&tc, 0, "-x", NULL);
  expect_opt(&tc, "x");
  test_verify(&tc);

  test_setup(&tc, 0, "-y", NULL);
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_setup(&tc, 0, "-x", "-y", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_setup(&tc, 0, "-xyz", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  test_verify(&tc);

  test_setup(&tc, 0, "-xyzzy", NULL);
  expect_opt(&tc, "x");
  expect_opt(&tc, "y");
  expect_opt(&tc, "z");
  expect_opt(&tc, "z");
  expect_opt(&tc, "y");
  test_verify(&tc);

  test_setup(&tc, 0, "-x", "nonopt", "-y", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "nonopt");
  expect_arg(&tc, "-y");
  test_verify(&tc);

  test_setup(&tc, 0, "-y", "--", "-x", NULL);
  expect_opt(&tc, "y");
  expect_arg(&tc, "-x");
  test_verify(&tc);

  test_setup(&tc, 0, "-z", "--", "-y", "nonopt", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt");
  test_verify(&tc);

  test_setup(&tc, 0, "-z", "--", "-y", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "z");
  expect_arg(&tc, "-y");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_setup(&tc, 0, "-x", "-", "nonopt1", "nonopt2", NULL);
  expect_opt(&tc, "x");
  expect_arg(&tc, "-");
  expect_arg(&tc, "nonopt1");
  expect_arg(&tc, "nonopt2");
  test_verify(&tc);

  test_setup(&tc, 0, "-q", NULL);
  expect_badopt(&tc, "q");
  test_verify(&tc);

  printf("TODO: write more thorough tests\n");
  return 0;
}
