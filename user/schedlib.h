// Helpers for scheduler experiments: describe a set of jobs, run them
// from a common start tick, and print the schedule as an ASCII chart.
//
// Include after kernel/types.h and user/user.h.

#include "kernel/param.h"
#include "kernel/schedlog.h"

#ifdef MLFQ_NLEVEL
#define NQUEUE MLFQ_NLEVEL
#else
#define NQUEUE 1
#endif

#define MAXJOBS  10
#define MAXOPS   24
#define SETUP    5  // ticks reserved for forking before the start tick
#define CHART_W  50 // ticks per chart row

#define OP_BURN  'b' // run on the CPU for n ticks
#define OP_PAUSE 'p' // sleep for n ticks (I/O)

struct op {
  char kind;
  int n;
};

struct job {
  const char *profile;
  int arrive;    // ticks after the start tick
  int level;     // first queue
  int priority;  // 0 is the highest
  int burst_est; // initial CPU burst estimate
  int nops;
  struct op ops[MAXOPS];
  // filled in by run_jobs()
  int pid;
};

static struct schedev evbuf[NSCHEDLOG];

// Spin until this process has run for n more ticks.
static inline void
burn(int n)
{
  int t0 = cputicks();
  while (cputicks() - t0 < n)
    ;
}

static inline void
addop(struct job *j, char kind, int n)
{
  if (j->nops < MAXOPS) {
    j->ops[j->nops].kind = kind;
    j->ops[j->nops].n = n;
    j->nops++;
  }
}

static inline int
jobindex(struct job *jobs, int njobs, int pid)
{
  for (int i = 0; i < njobs; i++)
    if (jobs[i].pid == pid)
      return i;
  return -1;
}

// Fork every job, run them, and collect the log.
// Returns the number of events in evbuf, and the start tick in *t0.
static inline int
run_jobs(struct job *jobs, int njobs, uint *t0)
{
  int done[2];
  char c;

  if (pipe(done) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }
  schedlog(0, 0); // discard old events
  uint start = uptime() + SETUP;

  for (int i = 0; i < njobs; i++) {
    int pid = fork();
    if (pid < 0) {
      fprintf(2, "fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      struct job *j = &jobs[i];
      close(done[0]);
      if (schedstart(j->level, j->priority, j->burst_est,
                     start + j->arrive) < 0) {
        fprintf(2, "schedstart failed (setup took longer than %d ticks?)\n",
                SETUP);
        exit(1);
      }
      for (int k = 0; k < j->nops; k++) {
        if (j->ops[k].kind == OP_BURN)
          burn(j->ops[k].n);
        else
          pause(j->ops[k].n);
      }
      exit(0);
    }
    jobs[i].pid = pid;
  }

  // read() returns 0 once every job has exited and closed its end.
  close(done[1]);
  read(done[0], &c, 1);
  close(done[0]);
  for (int i = 0; i < njobs; i++)
    wait(0);

  *t0 = start;
  return schedlog(evbuf, NSCHEDLOG);
}

//
// Output. printf has no field widths and writes one byte at a time,
// so lines are built in a buffer.
//

static char line[256];
static int linelen;

static inline void
lput(char c)
{
  if (linelen < sizeof(line) - 1)
    line[linelen++] = c;
}

static inline void
lstr(const char *s)
{
  while (*s)
    lput(*s++);
}

// Append n right-aligned in a field of width w.
static inline void
lnum(int n, int w)
{
  char tmp[12];
  int i = 0, neg = n < 0;

  if (neg)
    n = -n;
  do {
    tmp[i++] = '0' + n % 10;
    n /= 10;
  } while (n > 0);
  if (neg)
    tmp[i++] = '-';
  for (int k = i; k < w; k++)
    lput(' ');
  while (i > 0)
    lput(tmp[--i]);
}

// Pad the line with spaces up to column col.
static inline void
lpad(int col)
{
  while (linelen < col)
    lput(' ');
}

static inline void
lflush(void)
{
  line[linelen++] = '\n';
  write(1, line, linelen);
  linelen = 0;
}

static const char *evnames[] = {
  [SEV_NEW] = "new",         [SEV_RUN] = "run",       [SEV_PREEMPT] = "preempt",
  [SEV_REQUEUE] = "requeue", [SEV_SLEEP] = "sleep",   [SEV_WAKE] = "wake",
  [SEV_EXIT] = "exit",       [SEV_DEMOTE] = "demote", [SEV_PROMOTE] = "promote",
  [SEV_TICK] = "tick",
};

// One line per event, for graders: "ev <tick> <job> <event> L<level>".
static inline void
print_events(struct job *jobs, int njobs, int nev, uint t0)
{
  for (int i = 0; i < nev; i++) {
    struct schedev *e = &evbuf[i];
    int j = jobindex(jobs, njobs, e->pid);
    if (j < 0)
      continue;
    lstr("ev ");
    lnum(e->tick - t0, 0);
    lput(' ');
    lput('A' + j);
    lput(' ');
    lstr(evnames[e->ev]);
    lstr(" L");
    lnum(e->level, 0);
    lflush();
  }
}

// FNV-1a over the events, independent of absolute ticks and pids.
static inline uint
checksum(struct job *jobs, int njobs, int nev, uint t0)
{
  uint h = 2166136261u;

  for (int i = 0; i < nev; i++) {
    struct schedev *e = &evbuf[i];
    int j = jobindex(jobs, njobs, e->pid);
    int v[4] = { e->tick - t0, j, e->ev, e->level };
    for (int k = 0; k < 4; k++) {
      h ^= (uint)v[k];
      h *= 16777619u;
    }
  }
  return h;
}

// Gantt chart: one row per queue; a letter marks the job that ran
// during that tick.
static inline void
print_chart(struct job *jobs, int njobs, int nev, uint t0)
{
  int end = 0;

  for (int i = 0; i < nev; i++)
    if (evbuf[i].ev == SEV_TICK && (int)(evbuf[i].tick - t0) + 1 > end)
      end = evbuf[i].tick - t0 + 1;

  for (int base = 0; base < end; base += CHART_W) {
    int w = end - base < CHART_W ? end - base : CHART_W;
    lstr("tick");
    for (int t = 0; t < w; t++) {
      lpad(6 + t);
      if ((base + t) % 10 == 0)
        lnum(base + t, 0);
    }
    lflush();
    for (int l = 0; l < NQUEUE; l++) {
      lstr("L");
      lnum(l, 0);
      lpad(6);
      for (int t = 0; t < w; t++) {
        char c = '.';
        for (int i = 0; i < nev; i++) {
          struct schedev *e = &evbuf[i];
          if (e->ev == SEV_TICK && e->tick - t0 == base + t && e->level == l) {
            int j = jobindex(jobs, njobs, e->pid);
            if (j >= 0)
              c = 'A' + j;
          }
        }
        lput(c);
      }
      lflush();
    }
    lflush();
  }
}

// Per-job summary: when it arrived and finished, and how much CPU it used.
static inline void
print_summary(struct job *jobs, int njobs, int nev, uint t0)
{
  lstr("job  profile       arrive  finish  turnaround  cpu");
  lflush();
  for (int j = 0; j < njobs; j++) {
    int finish = -1, cpu = 0;
    for (int i = 0; i < nev; i++) {
      struct schedev *e = &evbuf[i];
      if (e->pid != jobs[j].pid)
        continue;
      if (e->ev == SEV_EXIT)
        finish = e->tick - t0;
      if (e->ev == SEV_TICK)
        cpu++;
    }
    lput(' ');
    lput('A' + j);
    lpad(5);
    lstr(jobs[j].profile);
    lpad(19);
    lnum(jobs[j].arrive, 6);
    lnum(finish, 8);
    lnum(finish - jobs[j].arrive, 12);
    lnum(cpu, 5);
    lflush();
  }
}
