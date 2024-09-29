/* copt.h - public domain command line option parsing library
   https://github.com/fardaniqbal/copt/

   Example usage:
   
   char *outfile = NULL;
   char *infile;
   struct copt opt = copt_init(argc, argv, 1);
   while (!copt_done(&opt)) {
     if (copt_opt(&opt, "a") {
       ... handle short option "-a" ...
     } else if (copt_opt(&opt, "longopt")) {
       ... handle long option "--longopt" ...
     } else if (copt_opt(&opt, "o|outfile")) {
       ... handle both short option "-o" and long option "--outfile" ...
       outfile = copt_arg(&opt);
     } else {
       fprintf(stderr, "unknown option '%s'\n", copt_curopt(&opt));
       usage();
     }
   }
   infile = argv[copt_idx(&opt)];
   ...etc... */
#ifndef COPT_H_INCLUDED_
#define COPT_H_INCLUDED_

/* Option parser's state.  Do not access fields directly. */
struct copt {
  char *curopt;
  char **argv;
  int argc;
  int idx;
  int subidx;
  int argidx;         /* index of opt's (potential) arg if reordering */
  char shortopt[3];
  unsigned reorder:1;
};

struct copt copt_init(int argc, char **argv, int reorder);
int    copt_done(struct copt *);          /* false if more opts remain */
int    copt_opt(const struct copt *, const char *); /* true if opt found */
char  *copt_arg(struct copt *);           /* return arg from last opt */
char  *copt_curopt(const struct copt *);  /* return most recent option */
int    copt_idx(const struct copt *);     /* current index into argv */
char  *copt_dbg_dump(void); /* INTERNAL DEBUG USE ONLY; NOT REENTRANT */

#endif /* COPT_H_INCLUDED_ */
