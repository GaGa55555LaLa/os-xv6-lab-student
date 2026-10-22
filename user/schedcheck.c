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
  { "arrival", "jobs run in the order they arrive",
    3, { { "a", 0, 0, 5, 2, 1, { B(3) } },
         { "b", 1, 0, 5, 2, 1, { B(1) } },
         { "c", 1, 0, 5, 2, 1, { B(2) } } } },
  { "no-preempt", "a running job keeps the CPU until it blocks or exits",
    2, { { "a", 0, 0, 5, 2, 1, { B(5) } },
         { "b", 1, 0, 5, 2, 1, { B(1) } } } },
  { "wake", "a job that wakes up joins the end of the queue",
    3, { { "io", 0, 0, 5, 2, 3, { B(1), P(1), B(1) } },
         { "b", 0, 0, 5, 2, 1, { B(3) } },
         { "c", 0, 0, 5, 2, 1, { B(3) } } } },
  { "tie", "same arrival time: the smaller pid runs first",
    2, { { "a", 0, 0, 5, 2, 1, { B(2) } },
         { "b", 0, 0, 5, 2, 1, { B(2) } } } },
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
