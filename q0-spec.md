## Introduction

Write a scheduler where processes take turns: each one runs for at most a short, fixed time, then goes to the back of the queue and the next one gets the CPU.

## Modifications (optional reading)

The following is already done; you do not need to implement it:

- The scheduling policy lives in `kernel/sched.c`. `struct proc` has new scheduling fields (`level`, `slice_used`, `wait_ticks`, `enter_tick`, ...).
- All processes are in one ready queue, queue 0. `pick_rr(level)` takes a `level` for the final problem, which has several queues; here it is always 0.
- `scheduler()` calls `sched_pick()`. A process that blocks or wakes up calls `sched_sleep()` / `sched_wakeup()`.
- `pause(n)` wakes up once, after n ticks.
- System calls: `schedstart(level, priority, burst_est, start_tick)`, `cputicks()`, `schedlog(buf, max)`.
- User programs: `schedtest`, `schedcheck`. Self-check: `./grade`.

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
$ schedcheck                   # 5 small fixed tests; schedcheck turns runs one of them
```

- Self-check: `./grade`, or `./grade turns alone` for some of the tests.
- `schedtest` prints the jobs, then a Gantt chart with one column per tick: a letter is the job running in that tick. Every job stays in row L0.

## Implementation

`kernel/sched.c` provides `pick_by(level, before)`. It returns the RUNNABLE process in queue `level` that should run first, where `before(a, b)` returns 1 if `a` should run before `b`.

Fill in the places marked `TODO`:

1. `TODO (q0)`: `pick_rr()` chooses the next process to run, and `should_preempt()` decides whether the running process must give up the CPU. Follow the rules below.
2. `TODO (trap)` (`kernel/trap.c`): tell the scheduler about every tick, and on a timer interrupt let it decide whether to give up the CPU. Do the accounting first, then decide. A timer interrupt in the kernel may come when no process is running. Until this is done, `pause()` never wakes up and the tests hang, so do it before testing anything else.
3. `TODO (yield)`: when `sched_timer()` gives up the CPU, put the process back at the end of the ready queue and switch to the scheduler.

The `TODO (trap)` and `TODO (yield)` parts are the same in every problem; you can copy them to the later problems.

### Already provided

- A new process joins the end of the ready queue.
- Every tick that `TODO (trap)` reports to the scheduler, the running process's time slice grows, and so does the waiting time of every RUNNABLE process. A `pause()` that is due wakes up.
- A process that wakes up joins the end of the ready queue.
- A process that is picked starts a new time slice.

### Scheduling rules

- The process that joined the ready queue earliest runs next. If several joined in the same tick, the smaller pid goes first.
- The time slice is `RR_SLICE` ticks (`kernel/param.h`).
- At each timer interrupt, after the accounting, the running process p does this:

| Case | Result |
| ---- | ------ |
| p has not used up its time slice | keep running |
| p has used up its time slice, and another process is RUNNABLE | give up the CPU, go to the end of the ready queue |
| p has used up its time slice, and no other process is RUNNABLE | keep running, and start a new time slice |

- Going to the end of the ready queue counts as joining it again now.
- In the last case, nothing else starts the new time slice; `should_preempt()` must do it.
- A process that blocks (for example in `pause()`) gives up the CPU by itself; you do not handle that.

## Rules

- Do not change (they are overwritten when grading): `user/` (you may add local tests there; they are overwritten), `mkfs/`, `Makefile`, `kernel/param.h`, `kernel/syscall.h`, `kernel/schedlog.h`, and the grading and environment files (`grade`, `test-xv6.py`, `.devcontainer/`, `lab`, `.gitattributes`).
- Do not add kernel source files.
- Do not change, add or remove any `schedlog()` / `logat()` call.

## Grading

This problem is worth 10 of the 100 implementation points.

| Part | Weight | How |
| ---- | ------ | --- |
| Public | 40% | `./grade` (5 tests) |
| Private | 60% | 10 hidden seeds of `schedtest` |

**Submission**
- The TAs grade the last commit on this problem's branch in your zip. Uncommitted changes are not graded.
- The TAs use their own copies of `./grade` and the protected files listed under Rules.
- Build failure: 0 for this problem.
- Kernel panic or hang: 0 for the affected test or seed.
- Hidden seed timeout: 60 s (a correct run takes about 15 s).

**Suggestions for Testing**

Passing `./grade` does not guarantee passing the hidden tests. To improve your confidence, we encourage you to:

- Add your own workloads to `user/schedcheck.c` and check the printed events against the rules by hand; your changes there are overwritten when grading.
- Try different `schedtest` seeds (e.g., `7`, `42`).
- Compare the Gantt charts with the scheduling rules by hand.

Hidden tests may include larger workloads, ties, and blocking not covered by the public tests.

## Using AI tools

- You may use generative AI tools (ChatGPT, Claude, Copilot, ...) for this lab.
- You are responsible for every line you submit. The lab quiz (4 of the 7 points) asks you to trace the scheduler and explain your implementation.
- Do not share your code with other students, and do not push it to a public repository.
