## Introduction

Combine the pickers from the four small problems into a four-level scheduler that moves processes up and down according to how they behave.

## Modifications (optional reading)

The following is already done; you do not need to implement it:

- The scheduling policy lives in `kernel/sched.c`. `struct proc` has new scheduling fields (`level`, `priority`, `burst_est`, `cur_burst`, `slice_used`, `allot_used`, `wait_ticks`, `enter_tick`, ...).
- `scheduler()` calls `sched_pick()`. A process that blocks or wakes up calls `sched_sleep()` / `sched_wakeup()`.
- `pause(n)` wakes up once, after n ticks.
- System calls: `schedstart(level, priority, burst_est, start_tick)`, `cputicks()`, `schedlog(buf, max)`.
- User programs: `schedtest`, `mlfqcheck`. Self-check: `./grade`.

## Instructions

### Environment

Run every command inside the container in the repo (`.devcontainer/`).

1. Install Docker.
2. Build the image (once):

```sh
docker build -t xv6-lab .devcontainer
```

3. Start the container:

```sh
docker run -it --rm --name xv6 \
    -u "$(id -u):$(id -g)" \
    -v "$PWD":/xv6 -w /xv6 \
    xv6-lab bash
```

On Windows, in PowerShell:

```powershell
docker run -it --rm --name xv6 `
    -v "${PWD}:/xv6" -w /xv6 `
    xv6-lab bash
```

- Shortcut: `./lab shell` builds the image if needed and starts the container; `./lab grade` and `./lab make qemu` run one command in it (Linux, macOS, or Git Bash/WSL on Windows).
- Quit QEMU: `Ctrl-a`, then `x`. Leave the container: `exit`.
- Windows: if `./grade` prints `env: $'python3\r': No such file or directory`, commit your changes first, then run:

```sh
git config core.autocrlf false
git rm -r -q --cached .
git reset -q --hard
```

- You can also open the repo with VS Code [Dev Containers](https://code.visualstudio.com/docs/devcontainers/containers).

### Build and run

```sh
make qemu                     # 1 CPU only
$ schedtest                    # draw a Gantt chart; schedtest 42 uses seed 42 for another workload
$ mlfqcheck                    # 20 small fixed tests
```

- Self-check: `./grade`.

### `schedtest` output

> **The output below is made up. It is not what really happens, and it even breaks the rules of this lab.** It only shows the format and what jobs A–H are. Trust your own run.

```
schedtest: seed 0

job  profile       arrive  prio  est  ops (bN = run N ticks, pN = sleep N ticks)
 A   interactive        0     5    1  b1 p4 b1 p5 b1 p3 ...
 B   io-bound           0     5    3  b3 p1 b4 p1 ...
 C   io-bound           1     5    3  b4 p1 b3 p1 ...
 D   cpu-bound          0     5   10  b30
 E   batch              3     2   10  b20
 F   batch-niced        3     4   10  b20
 G   late-command      12     5    2  b2
 H   phase-change       1     5   10  b17 b1 p3 b1 p2 ...

running...

tick  0         10        20
L0    DDDDDD....DDDD................
L1    ......EEFF....GG..............
L2    .........................HH...
L3    ................BBCCAAH....FE.

job  profile       arrive  finish  turnaround  cpu
 A   interactive        0      22          22    8
 ...

seed 0  checksum 00000000
```

- `arrive`: the tick the job starts being scheduled. `prio`: 0 is the highest. `est`: the initial burst estimate. `ops`: `bN` uses the CPU for N ticks, `pN` sleeps N ticks.
- Gantt chart: one column per tick, one row per queue. A letter is the job running in that queue.
- `turnaround = finish - arrive`. `cpu` is the number of CPU ticks used.

The kinds of A–H are fixed; the seed only changes their parameters:

| Job | Profile | Behavior |
| --- | ------- | -------- |
| A | interactive | 1 tick of CPU, then sleep; repeated |
| B, C | io-bound | a few ticks of CPU, then sleep 1 tick; repeated |
| D | cpu-bound | tens of ticks of CPU at once |
| E | batch | about 20 ticks of CPU at once |
| F | batch-niced | like E, with a lower priority |
| G | late-command | shows up late, 2 ticks of CPU |
| H | phase-change | one long CPU burst first, then 1 tick of CPU and sleep |

## Trace Code

This is in the scope of the lab quiz. No report is needed.

- How the timer interrupt works, and how the user-space and kernel-space handlers are switched:
  1. `prepare_return` -> `kernel/trampoline.S:uservec` -> `usertrap` -> `devintr` -> `clockintr`
  2. `usertrap` -> `kernel/kernelvec.S:kernelvec` -> `kerneltrap` -> `devintr` -> `clockintr`
- The hardware clock:
  1. The `timebase-frequency` of QEMU `virt` is 10000000 (`time` grows by 10 million per second). How long is one tick?
  2. When does the hardware raise a supervisor timer interrupt? Which line in `clockintr()` stops it from being pending?
  3. Comment out the line at the end of `clockintr()` that writes `stimecmp`, run `make qemu`, and explain what happens.
- Explain what each function on these paths does: which processes are in the ready queue, how their order is decided, how the scheduler picks one, and how the context switch happens:
  1. New -> Ready: `userinit` / `kfork` -> `allocproc`
  2. Running -> Ready: `usertrap` / `kerneltrap` -> `sched_timer` -> `sched` -> `kernel/swtch.S:swtch`
  3. Running -> Waiting: `sys_pause` -> `sleep_prepare` -> `sleep` -> `sched`
  4. Waiting -> Ready: `clockintr` -> `sched_tick` -> `sched_wakeup`
  5. Running -> Terminated: `sys_exit` -> `kexit` -> `sched`
  6. Ready -> Running: `scheduler` -> `sched_pick` -> `kernel/swtch.S:swtch`

## Implementation

`kernel/sched.c` provides `pick_by(level, before)` (the first RUNNABLE process in a queue, by a comparison function) and `top_level()` (the highest queue holding a RUNNABLE process).

Fill in the places marked `TODO`, in this order:

1. `TODO (q0)`–`TODO (q3)`: copy `pick_rr`, `pick_shortest`, `pick_priority`, `pick_fifo` and their helpers from q0–q3. Q2 ages with `MLFQ_AGE_STEP` instead.
2. `TODO (trap)` (`kernel/trap.c`): tell the scheduler about every tick, and let it decide on a timer interrupt whether to give up the CPU. On every timer interrupt, do the accounting first, then decide.
3. `TODO (yield)`: when `sched_timer()` gives up the CPU, put the process back in the ready queue and switch to the scheduler. You can copy it from the small problems, but you must handle `PREEMPT_KEEP` (keep its place; see Preemption).
4. `TODO (MLFQ)`: implement the rules below.

### Queues

- Q0 is the highest, Q3 the lowest. A RUNNABLE process with `level == k` is in Qk.
- While a higher queue is not empty, no process from a lower queue may run.

| Queue | Pick | Ties | Time slice | Allotment |
| ----- | ---- | ---- | ---------- | --------- |
| Q0 | the earliest to join | smaller pid | 1 tick | 2 ticks |
| Q1 | the least estimated remaining burst | smaller pid | — | 4 ticks |
| Q2 | the highest effective priority (smallest number) | smaller pid | 2 ticks | 8 ticks |
| Q3 | the earliest to join | smaller pid | — | unlimited |

- Allotment: the most ticks a CPU burst (running until it blocks) may use in one queue. It starts over in a new queue.
- Estimated remaining burst: the estimated burst minus the ticks this burst has run.
- Effective priority: `max(0, priority - wait_ticks / MLFQ_AGE_STEP)`, integer division. `priority` is 0–9, 0 is the highest.
- The parameters are the `MLFQ_*` constants in `kernel/param.h`. Do not change them.

### Already provided

- A new process joins the end of Q0.
- Every tick, the running process's burst and time slice grow, and so does the waiting time of every RUNNABLE process. A `pause()` that is due wakes up.
- A process that wakes up joins the end of its queue, and its waiting time is reset.
- A process that is picked starts a new time slice, and its waiting time is reset.

### Every tick

- The running process uses 1 tick of its allotment. If the allotment is used up and the process is not in Q3, it moves one queue down, to the end of that queue, and counts as "just demoted" for this tick.
- A RUNNABLE process that has waited at least 20 ticks in Q3 moves to the end of Q2. Its waiting time is reset, and its used allotment starts over.

### Blocking

- New estimated burst = (ticks this burst actually ran + old estimated burst) / 2, integer division.
- The used allotment starts over.
- Being preempted does not end a burst.

### Waking up

- Move up one queue, unless already in Q0.

### Preemption

After the accounting, the running process p is handled by this table, where top is the highest queue holding a RUNNABLE process:

| Case | Result |
| ---- | ------ |
| top is higher than p, p is not just demoted | give up the CPU, keep its place |
| top is higher than p, p is just demoted | give up the CPU, go to the end |
| top is p's queue, p is just demoted | give up the CPU, go to the end |
| p is in Q0, Q0 has another process, and the time slice is used up | give up the CPU, go to the end |
| p is in Q1, and Q1 has a process that comes before p | give up the CPU, go to the end |
| p is in Q2, the time slice is used up, and Q2 has a process that comes before p | give up the CPU, go to the end |
| otherwise (Q3 is never preempted from within Q3) | keep running |

- Keep its place: back in the queue at its old position; it does not count as joining again.
- Time slice used up but keeps running: start a new time slice.

## Rules

- Do not change (they are overwritten when grading): `user/`, `mkfs/`, `Makefile`, `kernel/param.h`, `kernel/syscall.h`, `kernel/schedlog.h`, and the grading and environment files (`grade`, `test-xv6.py`, `.devcontainer/`, `lab`, `.gitattributes`).
- Do not add kernel source files.
- Do not change, add or remove any `schedlog()` / `logat()` call.

## Grading

This xv6 project is worth **7 points** of the course grade: Implementation 3, Lab quiz 4.

### Implementation (3 points)

The five problems add up to 100 points, scaled to 3:

| Problem | Branch | Points | Public (40%) | Private (60%) |
| ------- | ------ | ------ | ------------ | ------------- |
| Problem 1 | `q0` | 10 | `./grade` (5 tests) | 10 hidden seeds |
| Problem 2 | `q1` | 10 | `./grade` (5 tests) | 10 hidden seeds |
| Problem 3 | `q2` | 10 | `./grade` (7 tests) | 10 hidden seeds |
| Problem 4 | `q3` | 10 | `./grade` (4 tests) | 10 hidden seeds |
| Multilevel feedback scheduler | `q4` | 60 | `./grade` (20 tests) | 10 hidden seeds |

**Submission and Grading**
- The TAs grade the last commit on this problem's branch in your zip. Uncommitted changes are not graded.
- The TAs use their own copies of `./grade` and the protected files listed under Rules.
- Build failure: 0 for this problem.
- Kernel panic or hang: 0 for the affected test or seed.
- Hidden seed timeout: 60 s (a correct run takes about 15 s).

**Suggestions for Testing**

Passing `./grade` does not guarantee passing the hidden tests. To improve your confidence, we encourage you to:

- Design additional edge cases and add them to your local copy of `grade`.
- Try different `schedtest` seeds (e.g., `7`, `42`).
- Compare the Gantt charts with the scheduling rules by hand.

Hidden tests may include larger workloads, ties, and blocking not covered by the public tests.

### Lab quiz (4 points)

Covers the Trace Code section, and may also cover your implementation.

## Using AI tools

- You may use generative AI tools (ChatGPT, Claude, Copilot, ...) for this lab.
- You are responsible for every line you submit. The lab quiz (4 of the 7 points) asks you to trace the scheduler and explain your implementation.
- Do not share your code with other students, and do not push it to a public repository.
