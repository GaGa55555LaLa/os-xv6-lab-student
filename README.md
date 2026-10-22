# xv6 Scheduling Lab

Each problem is on its own branch, with its own spec:

| Branch | Problem | Spec |
| ------ | ------- | ---- |
| `q0` | Problem 1 | `q0-spec.md` |
| `q1` | Problem 2 | `q1-spec.md` |
| `q2` | Problem 3 | `q2-spec.md` |
| `q3` | Problem 4 | `q3-spec.md` |
| `q4` | Multilevel feedback scheduler | `q4-spec.md` |

Do the problems in order: `q4` uses the pickers you write in `q0`–`q3`.

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

- The TAs rebuild and run the last commit on each problem's branch in your zip. Uncommitted changes are not graded.
- The TAs grade with their own copy of `./grade` and of the files listed under *Rules* in each spec, not the ones in your zip. A build failure gives 0 for that problem; a kernel panic or a hang gives 0 for that test or seed. Each hidden seed may run for up to 60 s (a correct run takes about 15 s).
- Passing `./grade` does not guarantee the hidden seeds: they use larger workloads with ties and blocking that the public tests may not hit. Check the Gantt chart of a few seeds (`schedtest 7`, `schedtest 42`) against the spec by hand.

### Lab quiz (4 points)

Covers the Trace Code section of `q4-spec.md` (branch `q4`), and may also cover your implementation.

## Getting started and submission

1. Clone this repo:

```sh
git clone https://github.com/GaGa55555LaLa/os-xv6-lab-student.git
```

   Everything runs in the course container: see *Environment* in `q0-spec.md` (or run `./lab shell` on a problem branch).

2. Do each problem on its branch (for example `git checkout q0`), following that branch's spec, and commit your work on that branch.
3. Zip the whole `os-xv6-lab-student` folder, including the hidden `.git` folder (it holds all your branches), as `<student_id>.zip`. Run `make clean` first to keep it small. From the folder that contains `os-xv6-lab-student`:

```sh
zip -r <student_id>.zip os-xv6-lab-student                 # macOS / Linux
tar -a -c -f <student_id>.zip os-xv6-lab-student           # Windows (PowerShell)
```

4. Upload `<student_id>.zip` to eLearn.

Do not push your work to a public repo (for example a fork of this one): everyone could see your code.

## Deadline

**2026/11/08 23:59 (UTC+8).** Your zip must be uploaded to eLearn by then.

## Using AI tools

- You may use generative AI tools (ChatGPT, Claude, Copilot, ...) for this lab.
- You are responsible for every line you submit. The lab quiz (4 of the 7 points) asks you to trace the scheduler and explain your implementation.
- Do not share your code with other students.
