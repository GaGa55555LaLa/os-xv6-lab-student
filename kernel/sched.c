// Scheduling policy: the pickers, per-tick accounting, preemption
// decisions, and the schedlog event buffer.
//
// Each picker chooses a RUNNABLE process from one queue (p->level); the
// scheduler stacks four queues, Q0 (highest) to Q3. See q4-spec.md
// for the rules.
//
// Fill in every place marked TODO:
//   TODO (q0) .. TODO (q3): copy your pickers from branches q0 .. q3.
//   TODO (MLFQ): the rules of this lab.
//   TODO (yield): sched_timer() gives up the CPU; copy it from q0 .. q3
//   and handle PREEMPT_KEEP.
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
// Copy your pickers from branches q0 .. q3, together with the helper
// functions they use. In this lab the priority queue ages by one step
// every MLFQ_AGE_STEP waiting ticks (not PRIO_AGE_STEP).
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
  // TODO (q3): copy pick_fifo() from branch q3.
  return 0;
}

// In ready-queue order, like pick_fifo().
struct proc *
pick_rr(int level)
{
  // TODO (q0): copy pick_rr() from branch q0.
  return 0;
}

// The process with the least estimated time left in its CPU burst.
struct proc *
pick_shortest(int level)
{
  // TODO (q1): copy pick_shortest() from branch q1.
  return 0;
}

// The process with the highest priority after aging.
struct proc *
pick_priority(int level)
{
  // TODO (q2): copy pick_priority() from branch q2, aging every
  // MLFQ_AGE_STEP waiting ticks.
  return 0;
}

// Choose the next process to run. Returns it locked and RUNNABLE, or 0.
struct proc *
sched_pick(void)
{
  struct proc *p;

  for (;;) {
    // TODO (MLFQ): look at the queues from Q0 to Q3 and take the first
    // process found: pick_rr() for Q0, pick_shortest() for Q1,
    // pick_priority() for Q2 and pick_fifo() for Q3. For now, only Q0.
    p = pick_rr(0);
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
  // TODO (MLFQ): update p->burst_est and reset the allotment p has used
  // in its queue (see "Blocking" in the spec).
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
  // TODO (MLFQ): coming back from sleep moves p up (see "Waking up" in
  // the spec).
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
  // TODO (MLFQ): count this tick against the allotment of p's queue.
  if (0 /* TODO (MLFQ): p has used up its allotment and is not in Q3 */) {
    // TODO (MLFQ): move p one queue down, to the end of that queue, and
    // remember that p was just demoted.
    schedlog(p, SEV_DEMOTE);
  }
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
      if (0 /* TODO (MLFQ): p has waited too long in Q3 */) {
        // TODO (MLFQ): move p up to the end of Q2.
        schedlog(p, SEV_PROMOTE);
      }
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

#define PREEMPT_KEEP    1 // back to the ready queue, keeping its place
#define PREEMPT_REQUEUE 2 // back to the end of the ready queue

// Highest (smallest) queue holding a RUNNABLE process, or MLFQ_NLEVEL.
static __attribute__((unused)) int
top_level(void)
{
  struct proc *p;
  int top = MLFQ_NLEVEL;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE && p->level < top)
      top = p->level;
    release(&p->lock);
  }
  return top;
}

// Should the running process p give up the CPU at this timer interrupt?
// Returns 0 (keep running), PREEMPT_KEEP or PREEMPT_REQUEUE.
static int
should_preempt(struct proc *p)
{
  // TODO (MLFQ): follow the "Preemption" table in the spec. Also start a
  // new time slice when p's slice is used up but p keeps running, and
  // forget that p was just demoted. Hold p->lock while you use p's fields, but not
  // while you call top_level() or a picker: they lock every process.
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
  // TODO (yield): p gives up the CPU. Put it back in the ready queue:
  // at the end of its queue for PREEMPT_REQUEUE, keeping its place for
  // PREEMPT_KEEP.
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

  if (level < 0 || level >= MLFQ_NLEVEL)
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
