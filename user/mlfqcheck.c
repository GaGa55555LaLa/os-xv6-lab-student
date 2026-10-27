// mlfqcheck [test]
//
// Small fixed workloads, one per MLFQ rule. Each job can start in any
// queue, so a rule can be tested on its own. Prints every scheduler
// event; grade compares them with the expected events.

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
  { "demote", "a long CPU burst sinks one queue each time it uses up the queue's allotment",
    1, { { "cpu", 0, 0, 5, 2, 1, { B(16) } } } },
  { "rr", "L0 runs its jobs in turn, one tick each",
    3, { { "a", 0, 0, 5, 2, 1, { B(2) } },
         { "b", 0, 0, 5, 2, 1, { B(2) } },
         { "c", 0, 0, 5, 2, 1, { B(2) } } } },
  { "shortest", "L1 runs the job with the least estimated time left",
    3, { { "est6", 0, 1, 5, 6, 1, { B(3) } },
         { "est2", 0, 1, 5, 2, 1, { B(3) } },
         { "est4", 0, 1, 5, 4, 1, { B(3) } } } },
  { "shortest-preempt", "a job with less estimated time left takes over L1 at once",
    2, { { "est6", 0, 1, 5, 6, 1, { B(4) } },
         { "est1", 2, 1, 5, 1, 1, { B(1) } } } },
  { "shortest-tie", "equal estimated time left in L1: the smaller pid runs first, not the earlier arrival",
    3, { { "first", 0, 1, 5, 1, 1, { B(3) } },
         { "later", 2, 1, 5, 3, 1, { B(1) } },
         { "earlier", 1, 1, 5, 3, 1, { B(1) } } } },
  { "estimate", "after a burst, the estimate becomes (ticks run + old estimate) / 2, rounded down",
    3, { { "est5", 2, 1, 5, 5, 1, { B(2) } },
         { "est3", 2, 1, 5, 3, 1, { B(2) } },
         { "io", 0, 2, 5, 8, 3, { B(1), P(1), B(2) } } } },
  { "progress", "time left = estimate - ticks run in this burst; being preempted does not end the burst",
    3, { { "est6", 0, 1, 5, 6, 1, { B(4) } },
         { "l0", 2, 0, 5, 2, 1, { B(1) } },
         { "est5", 2, 1, 5, 5, 1, { B(2) } } } },
  { "preempt-estimate", "being preempted does not update the estimate",
    3, { { "est4", 0, 1, 5, 4, 1, { B(3) } },
         { "l0", 1, 0, 5, 2, 1, { B(1) } },
         { "est2", 1, 1, 5, 2, 1, { B(1) } } } },
  { "priority", "L2 runs the highest priority, and waiting raises priority",
    3, { { "prio1", 0, 2, 1, 2, 1, { B(6) } },
         { "prio3", 0, 2, 3, 2, 1, { B(6) } },
         { "prio5", 0, 2, 5, 2, 1, { B(6) } } } },
  { "priority-tie", "equal priority in L2: the smaller pid runs first, not the earlier arrival",
    3, { { "first", 0, 2, 0, 2, 1, { B(2) } },
         { "later", 2, 2, 2, 2, 1, { B(1) } },
         { "earlier", 1, 2, 2, 2, 1, { B(1) } } } },
  { "slice", "an L2 job alone starts a new time slice when its slice is used up",
    2, { { "alone", 0, 2, 5, 2, 1, { B(6) } },
         { "prio1", 3, 2, 1, 2, 1, { B(1) } } } },
  { "fifo", "L3 runs jobs in arrival order; a job preempted by a higher queue keeps its place",
    3, { { "first", 0, 3, 5, 2, 1, { B(4) } },
         { "second", 0, 3, 5, 2, 1, { B(2) } },
         { "l0", 2, 0, 5, 2, 1, { B(1) } } } },
  { "preempt", "a job in a higher queue preempts the running job",
    3, { { "l3", 0, 3, 5, 2, 1, { B(5) } },
         { "l2", 1, 2, 5, 2, 1, { B(3) } },
         { "l1", 2, 1, 5, 2, 1, { B(2) } } } },
  { "starve", "a job that waits too long in L3 moves up to L2",
    4, { { "l3", 0, 3, 5, 2, 1, { B(3) } },
         { "a", 0, 2, 5, 2, 1, { B(8) } },
         { "b", 0, 2, 5, 2, 1, { B(8) } },
         { "c", 0, 2, 5, 2, 1, { B(8) } } } },
  { "wake", "waking up from sleep moves a job one queue up",
    1, { { "io", 0, 3, 5, 2, 7, { B(1), P(1), B(1), P(1), B(1), P(1), B(1) } } } },
  { "wake-tail", "a job that wakes up joins the end of its queue",
    3, { { "io", 0, 0, 5, 2, 3, { B(1), P(2), B(1) } },
         { "b", 2, 0, 5, 2, 1, { B(1) } },
         { "c", 2, 0, 5, 2, 1, { B(1) } } } },
  { "priority-age-tie", "an aged L2 tie preempts a larger pid when the time slice expires",
    2, { { "small-pid", 0, 2, 3, 2, 1, { B(3) } },
         { "large-pid", 1, 2, 1, 2, 1, { B(6) } } } },
  { "priority-pid-not-wait", "on an aged L2 tie, smaller pid beats longer waiting time",
    3, { { "small-pid", 5, 2, 1, 2, 1, { B(1) } },
         { "long-wait", 0, 2, 4, 2, 1, { B(2) } },
         { "cpu-holder", 0, 2, 0, 2, 1, { B(6) } } } },
  { "estimate-history", "the estimate uses the latest burst, not total CPU ticks across bursts",
    2, { { "io", 0, 3, 5, 4, 5, { B(4), P(1), B(2), P(1), B(1) } },
         { "est4", 8, 1, 5, 4, 1, { B(1) } } } },
  { "remaining-history", "L1 remaining time subtracts ticks in this burst, not total CPU ticks",
    2, { { "io", 0, 3, 5, 4, 5, { B(4), P(1), B(2), P(1), B(1) } },
         { "est2", 8, 1, 5, 2, 1, { B(1) } } } },
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
