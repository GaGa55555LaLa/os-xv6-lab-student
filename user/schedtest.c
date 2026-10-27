// schedtest [seed] [-v]
//
// Runs a mix of jobs that behave like real programs, with lengths and
// arrival times derived from the seed (default DEFAULT_SEED), and prints
// the schedule. -v also prints every scheduler event (used by the grader).

#include "kernel/types.h"
#include "user/user.h"
#include "user/schedlib.h"

#define DEFAULT_SEED "1"

static uint rng;

static int
rnd(int lo, int hi)
{
  rng = rng * 1103515245u + 12345u;
  return lo + (rng >> 16) % (hi - lo + 1);
}

static uint
hash(const char *s)
{
  uint h = 2166136261u;
  for (; *s; s++) {
    h ^= (uchar)*s;
    h *= 16777619u;
  }
  return h;
}

static struct job *
newjob(struct job *jobs, int *n, const char *profile, int arrive, int prio,
       int est)
{
  struct job *j = &jobs[(*n)++];
  memset(j, 0, sizeof(*j));
  j->profile = profile;
  j->arrive = arrive;
  j->level = 0; // new processes always start in the top queue
  j->priority = prio;
  j->burst_est = est;
  return j;
}

static int
make_jobs(struct job *jobs)
{
  struct job *j;
  int n = 0;

  // A: shell or editor waiting for keystrokes.
  j = newjob(jobs, &n, "interactive", 0, DEF_PRIORITY, 1);
  for (int i = 0; i < 8; i++) {
    addop(j, OP_BURN, 1);
    addop(j, OP_PAUSE, rnd(3, 6));
  }

  // B, C: compiler or grep, short CPU bursts between reads.
  j = newjob(jobs, &n, "io-bound", 0, DEF_PRIORITY, 3);
  for (int i = 0; i < 5; i++) {
    addop(j, OP_BURN, rnd(3, 4));
    addop(j, OP_PAUSE, 1);
  }
  j = newjob(jobs, &n, "io-bound", rnd(1, 2), DEF_PRIORITY, 3);
  for (int i = 0; i < 5; i++) {
    addop(j, OP_BURN, rnd(3, 5));
    addop(j, OP_PAUSE, 1);
  }

  // D: compression or number crunching.
  j = newjob(jobs, &n, "cpu-bound", 0, DEF_PRIORITY, 10);
  addop(j, OP_BURN, rnd(28, 36));

  // E, F: two batch jobs; the user gave E the higher priority (nice).
  // (one rnd() per statement: argument evaluation order is unspecified)
  int hi = rnd(1, 3);
  int arrive = rnd(2, 4);
  j = newjob(jobs, &n, "batch", arrive, hi, 10);
  addop(j, OP_BURN, rnd(18, 22));
  arrive = rnd(2, 4);
  int lo = hi + rnd(2, 3);
  j = newjob(jobs, &n, "batch-niced", arrive, lo, 10);
  addop(j, OP_BURN, rnd(18, 22));

  // G: a short command typed while the batch jobs are running.
  j = newjob(jobs, &n, "late-command", rnd(60, 80), DEF_PRIORITY, 2);
  addop(j, OP_BURN, 2);

  // H: crunches numbers, then turns interactive.
  j = newjob(jobs, &n, "phase-change", 1, DEF_PRIORITY, 10);
  addop(j, OP_BURN, rnd(16, 20));
  for (int i = 0; i < 4; i++) {
    addop(j, OP_BURN, 1);
    addop(j, OP_PAUSE, rnd(2, 4));
  }

  return n;
}

static void
print_jobs(struct job *jobs, int njobs)
{
  lstr("job  profile       arrive  prio  est  ops (bN = run N ticks, pN = sleep N ticks)");
  lflush();
  for (int i = 0; i < njobs; i++) {
    struct job *j = &jobs[i];
    lput(' ');
    lput('A' + i);
    lpad(5);
    lstr(j->profile);
    lpad(19);
    lnum(j->arrive, 6);
    lnum(j->priority, 6);
    lnum(j->burst_est, 5);
    lpad(38);
    for (int k = 0; k < j->nops; k++) {
      if (linelen > 70) {
        lflush();
        lpad(38);
      }
      lput(j->ops[k].kind);
      lnum(j->ops[k].n, 0);
      lput(' ');
    }
    lflush();
  }
}

int
main(int argc, char *argv[])
{
  static struct job jobs[MAXJOBS];
  char *seed = 0;
  int verbose = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      verbose = 1;
    } else if (argv[i][0] == '-' || seed != 0) {
      fprintf(2, "usage: schedtest [seed] [-v]\n");
      exit(1);
    } else {
      seed = argv[i];
    }
  }
  if (seed == 0)
    seed = DEFAULT_SEED;

  rng = hash(seed);
  int njobs = make_jobs(jobs);

  printf("schedtest: seed %s\n\n", seed);
  print_jobs(jobs, njobs);
  printf("\nrunning...\n\n");

  uint t0;
  int nev = run_jobs(jobs, njobs, &t0);
  if (nev < 0) {
    fprintf(2, "schedlog failed\n");
    exit(1);
  }

  if (verbose)
    print_events(jobs, njobs, nev, t0);
  print_chart(jobs, njobs, nev, t0);
  print_summary(jobs, njobs, nev, t0);
  printf("\nseed %s  checksum %x\n", seed, checksum(jobs, njobs, nev, t0));
  exit(0);
}
