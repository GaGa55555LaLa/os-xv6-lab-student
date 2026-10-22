// Scheduling policy: the picker, per-tick accounting, the preemption
// decision, and the schedlog event buffer.
//
// The picker chooses a RUNNABLE process from one queue (p->level). This
// lab uses only queue 0; the final lab stacks four queues.
//
// Fill in every place marked TODO:
//   TODO (q3): pick_fifo() and should_preempt().
//   TODO (yield): sched_timer() gives up the CPU.
//   TODO (trap) in kernel/trap.c: connect the timer interrupt to the scheduler.
// Do not add, remove or change any schedlog() or logat() call.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "schedlog.h"

extern struct proc proc[NPROC];

//
// schedlog: a fixed-size buffer of events, read by sys_schedlog().
//

static struct spinlock loglock;
static struct schedev logbuf[NSCHEDLOG];
static int nlog;

void
schedinit(void)
{
  initlock(&loglock, "schedlog");
}

static void
logat(struct proc *p, int ev, uint tick)
{
  if (!p->logenabled)
    return;
  acquire(&loglock);
  if (nlog < NSCHEDLOG) {
    logbuf[nlog].tick = tick;
    logbuf[nlog].pid = p->pid;
    logbuf[nlog].ev = ev;
    logbuf[nlog].level = p->level;
    nlog++;
  } else if (nlog == NSCHEDLOG) {
    printk("schedlog: buffer full, dropping events\n");
    nlog++;
  }
  release(&loglock);
}

// Record an event for p. Caller must hold p->lock.
void
schedlog(struct proc *p, int ev)
{
  logat(p, ev, ticks);
}

// Copy up to max events to user address addr and empty the buffer.
// Returns the number of events copied, or -1.
int
schedlog_read(uint64 addr, int max)
{
  int n;

  acquire(&loglock);
  n = nlog < NSCHEDLOG ? nlog : NSCHEDLOG;
  if (n > max)
    n = max;
  if (n > 0 && copyout(myproc()->pagetable, myproc()->sz, addr, (char *)logbuf,
                       n * sizeof(struct schedev)) < 0) {
    release(&loglock);
    return -1;
  }
  nlog = 0;
  release(&loglock);
  return n;
}

//
// Pickers. Each returns the best RUNNABLE process in the given queue,
// or 0. The result is not locked; the caller must lock it and check
// that it is still RUNNABLE.
//

// The best RUNNABLE process in queue level, where before(a, b) says
// whether a should run before b.
static __attribute__((unused)) struct proc *
pick_by(int level, int (*before)(struct proc *, struct proc *))
{
  struct proc *p, *best = 0;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE && p->level == level &&
        (best == 0 || before(p, best)))
      best = p;
    release(&p->lock);
  }
  return best;
}

// The process that has been in the ready queue the longest.
struct proc *
pick_fifo(int level)
{
  // TODO (q3): the RUNNABLE process in queue level that runs next.
  return 0;
}

// Choose the next process to run. Returns it locked and RUNNABLE, or 0.
struct proc *
sched_pick(void)
{
  struct proc *p;

  for (;;) {
    p = pick_fifo(0);
    if (p == 0)
      return 0;
    acquire(&p->lock);
    if (p->state == RUNNABLE)
      return p;
    release(&p->lock);
  }
}

//
// State-change hooks, called from proc.c with p->lock held.
//

// p is about to block: its CPU burst is over.
void
sched_sleep(struct proc *p)
{
  if (p->starting)
    return;
  p->cur_burst = 0;
  schedlog(p, SEV_SLEEP);
}

// Make a SLEEPING process RUNNABLE.
void
sched_wakeup(struct proc *p)
{
  p->state = RUNNABLE;
  p->enter_tick = ticks;
  p->wait_ticks = 0;
  if (p->starting) {
    p->starting = 0;
    p->logenabled = 1;
    schedlog(p, SEV_NEW);
    return;
  }
  schedlog(p, SEV_WAKE);
}

//
// Timer.
//

// Charge the tick that just ended to a RUNNING process.
static void
charge(struct proc *p)
{
  p->rticks++;
  p->slice_used++;
  p->cur_burst++;
  logat(p, SEV_TICK, ticks - 1);
}

// Called by clockintr() on CPU 0 right after ticks++.
void
sched_tick(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNING) {
      charge(p);
    } else if (p->state == RUNNABLE) {
      p->wait_ticks++;
    }
    // wake up pause() and schedstart() sleepers whose time has come.
    if (p->chan == &p->wake_tick && ticks >= p->wake_tick) {
      p->chan = 0;
      if (p->state == SLEEPING)
        sched_wakeup(p);
    }
    release(&p->lock);
  }
}

#define PREEMPT_REQUEUE 2 // back to the end of the ready queue

// Should the running process p give up the CPU at this timer interrupt?
static int
should_preempt(struct proc *p)
{
  // TODO (q3): must p give up the CPU now? Return 0 to keep
  // running, or PREEMPT_REQUEUE.
  return 0;
}

// Called from usertrap() and kerneltrap() on a timer interrupt.
void
sched_timer(void)
{
  struct proc *p = myproc();
  int r = should_preempt(p);

  if (r == 0)
    return;
  acquire(&p->lock);
  // TODO (yield): p gives up the CPU. Put it back in the ready queue,
  // at the end of the queue.
  schedlog(p, r == PREEMPT_REQUEUE ? SEV_REQUEUE : SEV_PREEMPT);
  // TODO (yield): switch to the scheduler.
  release(&p->lock);
}

// schedstart(): set up the caller's scheduling state, then sleep until
// ticks reaches start, when it joins the ready queue and logging begins.
int
schedstart(int level, int priority, int burst_est, uint start)
{
  struct proc *p = myproc();

  if (level != 0)
    return -1;
  acquire(&tickslock);
  if (ticks >= start) {
    release(&tickslock);
    return -1;
  }
  acquire(&p->lock);
  p->level = level;
  p->priority = priority;
  p->burst_est = burst_est;
  p->cur_burst = 0;
  p->allot_used = 0;
  p->rticks = 0;
  p->starting = 1;
  p->wake_tick = start;
  release(&p->lock);
  sleep_prepare(&p->wake_tick);
  release(&tickslock);
  sleep();

  // the start tick passed before we got to sleep().
  acquire(&p->lock);
  if (p->starting) {
    p->starting = 0;
    p->logenabled = 1;
    p->enter_tick = ticks;
    schedlog(p, SEV_NEW);
  }
  release(&p->lock);
  return 0;
}
