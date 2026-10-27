// Scheduler event log, shared by the kernel and user programs.
// Only processes started with schedstart() are logged.

#define SEV_NEW     1 // started by schedstart(); joins its first queue
#define SEV_RUN     2 // picked by the scheduler
#define SEV_PREEMPT 3 // preempted by a higher queue; keeps its place
#define SEV_REQUEUE 4 // slice over or lost to a better process; goes to the back
#define SEV_SLEEP   5 // blocked (end of a CPU burst)
#define SEV_WAKE    6 // woken up
#define SEV_EXIT    7
#define SEV_DEMOTE  8 // used up its allotment, moved one queue down
#define SEV_PROMOTE 9 // waited too long in the lowest queue, moved up
#define SEV_TICK    10 // was running during the tick that just ended

struct schedev {
  uint tick;   // value of ticks when the event happened (SEV_TICK: the tick that ran)
  int pid;
  short ev;    // SEV_*
  short level; // queue after the event (SEV_TICK: queue while running)
};
