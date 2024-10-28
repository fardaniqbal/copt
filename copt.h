/* copt - public domain C/C++ command line option parsing library

   Why should you use this instead of other command line parsing libraries?
   How do you even use this?  These questions, and more, are answered here:
   https://github.com/fardaniqbal/copt/

   Example usage:
   
   int got_a = 0;
   int got_withducks = 0;
   char *color = "default";
   char *out = NULL;
   char *in;

   struct copt opt = copt_init(argc, argv, 1);
   while (copt_next(&opt)) {
     if (copt_opt(&opt, "a") {
       got_a = 1;              // found -a (maybe grouped, e.g. -xyaz)
     } else if (copt_opt(&opt, "withducks")) {
       got_withducks = 1;      // found --withducks
     } else if (copt_opt(&opt, "o|outfile")) {
       out = copt_arg(&opt);   // found -o, -oARG, -o ARG, -o=ARG, -xyoARG,
                               // --outfile ARG, --outfile=ARG, etc
     } else if (copt_opt(&opt, "c|color=")) {
       color = copt_arg(&opt); // same, but _disallow space_ between option
                               // and ARG (--color=ARG, _not_ --color ARG)
     } else {
       fprintf(stderr, "unknown option '%s'\n", copt_curopt(&opt));
       usage();                // found unknown opt
     }
   }
   in = argv[copt_idx(&opt)];  // copt_idx() gives first non-option arg
   ...etc... */
#ifndef COPT_H_INCLUDED_
#define COPT_H_INCLUDED_
#ifdef __cplusplus
extern "C" {
#endif

/* Option parser's state.  Do not access fields directly. */
struct copt {
  char *curopt;
  char **argv;
  int argc;
  int idx;            /* current index into argv */
  int subidx;         /* > 0 if in grouped short opts */
  int argidx;         /* index of opt's (potential) arg if reordering */
  char shortopt[3];   /* to get last short opt even if grouped */
  unsigned reorder:1; /* true if allowing opts mixed with non-opts */
};

struct copt copt_init(int argc, char **argv, int reorder);
int    copt_next(struct copt *);          /* true if more opts remain */
int    copt_opt(struct copt *, const char *); /* true if opt found */
char  *copt_arg(struct copt *);           /* return arg from last opt */
char  *copt_curopt(const struct copt *);  /* return most recent option */
int    copt_idx(const struct copt *);     /* current index into argv */
char  *copt_dbg_dump(void); /* INTERNAL DEBUG USE ONLY; NOT REENTRANT */

#ifdef __cplusplus
}
#endif
#endif /* COPT_H_INCLUDED_ */
