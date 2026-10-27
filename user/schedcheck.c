// schedcheck [test]
//
// Small fixed workloads, one per scheduling rule. Prints every
// scheduler event; grade compares them with the expected events.

#include "kernel/types.h"
#include "user/user.h"
#include "user/schedlib.h"

struct test {
  const char *name;
  const char *what;
  int njobs;
  struct job jobs[4];
};

#define B(n) { OP_BURN, n }
#define P(n) { OP_PAUSE, n }

// job: profile, arrive, level, priority, burst_est, nops, ops
static struct test tests[] = {
  { "priority", "the highest priority (smallest number) runs first",
    3, { { "prio1", 0, 0, 1, 2, 1, { B(4) } },
         { "prio3", 0, 0, 3, 2, 1, { B(4) } },
         { "prio5", 0, 0, 5, 2, 1, { B(4) } } } },
  { "preempt", "a job with a higher priority takes the CPU at once",
    2, { { "prio5", 0, 0, 5, 2, 1, { B(4) } },
         { "prio1", 1, 0, 1, 2, 1, { B(2) } } } },
  { "aging", "waiting raises priority, so a low-priority job gets to run",
    2, { { "prio1", 0, 0, 1, 2, 1, { B(12) } },
         { "prio3", 0, 0, 3, 2, 1, { B(2) } } } },
  { "tie", "equal priority: the smaller pid runs first",
    2, { { "a", 0, 0, 2, 2, 1, { B(2) } },
         { "b", 0, 0, 2, 2, 1, { B(2) } } } },
  { "tie-arrival", "equal priority: the smaller pid runs first, not the earlier arrival",
    3, { { "first", 0, 0, 0, 2, 1, { B(2) } },
         { "later", 2, 0, 2, 2, 1, { B(1) } },
         { "earlier", 1, 0, 2, 2, 1, { B(1) } } } },
  { "age-tie-preempt", "aging into an equal priority preempts a larger pid",
    2, { { "small-pid", 0, 0, 4, 2, 1, { B(2) } },
         { "large-pid", 1, 0, 1, 2, 1, { B(12) } } } },
  { "age-pid-not-wait", "on an aged tie, smaller pid beats longer waiting time",
    3, { { "small-pid", 5, 0, 1, 2, 1, { B(2) } },
         { "long-wait", 0, 0, 3, 2, 1, { B(2) } },
         { "cpu-holder", 0, 0, 0, 2, 1, { B(6) } } } },
};

#define NTESTS (sizeof(tests) / sizeof(tests[0]))

static void
runtest(struct test *t)
{
  uint t0;

  printf("test %s: %s\n", t->name, t->what);
  int nev = run_jobs(t->jobs, t->njobs, &t0);
  if (nev < 0) {
    fprintf(2, "schedlog failed\n");
    exit(1);
  }
  print_events(t->jobs, t->njobs, nev, t0);
  printf("end %s\n", t->name);
}

int
main(int argc, char *argv[])
{
  for (int i = 0; i < NTESTS; i++)
    if (argc < 2 || strcmp(argv[1], tests[i].name) == 0)
      runtest(&tests[i]);
  exit(0);
}
