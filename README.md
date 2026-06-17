# OS Project – Traffic Simulation on a Directed Graph
Semester project for the Operating Systems course

---

## Team Members and Responsibilities

<<<<<<< HEAD
| Member | Role | Files |
|---|---|---|
| **Shira Kraft** | Algorithms / Path Logic, Passenger Process (M5–M6), Makefile, README | `Dijkstra.c`, `milestone5.c`, `Makefile`, `README.md`, `tester.c` |
| **Bat-El Zairi** | Graph Setup & Data Management | `graph.c`, `graph.h` |
| **Aviya Ben David** | System, GUI & Quality Assurance | `gui.c`, `gui.h`, `main.c` |
=======
Shira Kraft - Algorithms / Path Logic, QA & Documentation
  Dijkstra implementation, path reconstruction, edge cases,
  test suite (tester.c, test.sh), input validation, README
  Files: Dijkstra.c, gui.h, gui.c, tester.c, test.sh
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3

---

## Project Description

<<<<<<< HEAD
This project implements a directed weighted graph simulation in C.  
The system finds the shortest path between nodes using Dijkstra's algorithm and visualises the graph using the **raylib** library.
=======
Hila - Synchronisation Architecture & Locking Mechanisms
  Semaphore-based critical sections, ensuring only one traveler
  occupies a node at a time, deadlock and starvation prevention
  Files: milestone5.c, gui_m6.c, gui_m6.h, main_m6.c

>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3

The graph represents a city traffic system where each node is an intersection and each directed edge is a road whose weight represents travel time in minutes.

---

## Tech Stack

- **Language:** C (C11)  
- **Environment:** Linux  
- **GUI Library:** raylib  
- **Version Control:** GitHub  

---

## File Structure

| File | Description | Owner |
|---|---|---|
| `main.c` | Entry point (milestones 1–3) | Aviya Ben David |
| `main_m4.c` | Entry point (milestone 4) | Aviya Ben David |
| `main_m5.c` | Entry point (milestone 5) | Aviya Ben David |
| `main_m6.c` | Entry point (milestone 6) | Aviya Ben David |
| `graph.c / graph.h` | Adjacency-list graph, file parser | Bat-El Zairi |
| `Dijkstra.c` | Dijkstra, path reconstruction | Shira Kraft |
| `milestone4.c / .h` | M4 child process logic | Shira Kraft |
| `milestone5.c / .h` | M5–M6 autonomous child process, semaphore sync | Shira Kraft |
| `child.c / .h` | M4 child utilities | Shira Kraft |
| `child_ipc.c / .h` | Child-side pipe IPC API | Shira Kraft |
| `parent_ipc.c / .h` | Parent-side pipe IPC API | Aviya Ben David |
| `ipc_protocol.h` | Shared `StatusMessage` struct, `SyncState` enum | Aviya Ben David |
| `gui.c / .h` | Raylib GUI (M1–M4) | Aviya Ben David |
| `gui_parent.c / .h` | Raylib GUI (M5) | Aviya Ben David |
| `gui_m6.c / .h` | Raylib GUI (M6, shows waiting queue) | Aviya Ben David |
| `signal_handler.c / .h` | SIGCHLD / SIGINT handler | Aviya Ben David |
| `tester.c` | Automated test suite (12 tests) | Shira Kraft |
| `Makefile` | Build configuration | Shira Kraft |

---

## How to Build and Run

### Build all milestones
```bash
make milestone1   # terminal Dijkstra only
make milestone2   # static GUI
make milestone3   # animated GUI (300 ms/step, 1 s node pause)
make milestone4   # multi-process, no IPC
make milestone5   # autonomous children + pipe IPC
make milestone6   # node synchronisation with POSIX semaphores
```

### Run (milestones 2–6)
```bash
./sim <input_file>
```

### Run (milestone 1)
```bash
./dijkstra <input_file>
```

---

## Input File Format

```
N M
src1 dst1 weight1
src2 dst2 weight2
...
[travelers section for M4+]
```

**Example (6 nodes, 8 edges, one traveler):**
```
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 10
0 5
```

---

## Animation Timing (Milestone 3)

| Parameter | Value | Macro |
|---|---|---|
| Step delay along edge | 300 ms | `STEP_DELAY_MS` |
| Node arrival pause | 1 000 ms | `NODE_WAIT_MS` |

Override without editing the Makefile:
```bash
make milestone3 STEP_DELAY_MS=500 NODE_WAIT_MS=2000
```

---

## Running Tests

```bash
make test
# or manually:
gcc -o tester tester.c graph.c -lm
./tester ./dijkstra
```

---

## Milestones

| # | Description | Target |
|---|---|---|
| 1 | Graph representation and Dijkstra | `milestone1` |
| 2 | GUI, graph visualisation | `milestone2` |
| 3 | Movement animation on graph | `milestone3` |
| 4 | Multiple processes and parent | `milestone4` |
| 5 | Inter-process communication | `milestone5` |
| 6 | Synchronisation | `milestone6` |
| 7 | Scheduling algorithms | `milestone7` |

---

## Milestone 4 – Multi-Process Architecture

### Compilation and Run
```bash
make milestone4
./sim imput.txt
```
<<<<<<< HEAD
=======
Design Overview
In this milestone, the simulation transitions to a multi-process architecture.
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3

### Design Overview
- **Parent Process:** Parses the extended input file, computes paths for all travelers using Dijkstra, forks child processes, and runs the Raylib GUI loop rendering all travelers simultaneously in different colours. Reaps all terminated children before exiting.
- **Child Processes:** Each child represents one traveler. On creation it prints `[PID] started` and exits, delegating movement management to the parent.

<<<<<<< HEAD
---

## Milestone 5 – Inter-Process Communication (IPC)

### Compilation and Run
```bash
make milestone5
./sim <input_file>
```

### IPC Mechanism – Anonymous Pipes

Each child communicates with the parent through a **dedicated anonymous pipe** (one pipe per traveler), created with `pipe()` before `fork()`.

#### Message Format – `StatusMessage` (`ipc_protocol.h`)

| Field | Type | Meaning |
|---|---|---|
| `pid` | `pid_t` | Child PID |
| `current_node` | `int` | Node the child is currently at |
| `next_node` | `int` | Node the child is heading to |
| `target_node` | `int` | Node the child wants to enter (M6) |
| `sync_state` | `int` | `SYNC_MOVING / SYNC_WAITING / SYNC_INSIDE` |
| `is_destination` | `bool` | True when `current_node` is the final stop |
| `is_finished` | `bool` | True when the journey is over |

#### Why pipes?

- **Simplicity:** created before `fork()`, inherited automatically, no shared-memory setup needed.
- **Atomicity:** `sizeof(StatusMessage)` < `PIPE_BUF` (4 096 B on Linux), so each `write()` is atomic – sibling children cannot interleave their messages.
- **Clean EOF:** when a child closes its write end, the parent's `read()` returns 0, signalling completion without extra bookkeeping.

#### File-Descriptor Hygiene

After `fork()`, each child calls `init_child_ipc_side(index)` which:
1. Retains only its own pipe's **write end**.
2. Closes all other children's pipe fds (both ends) to prevent stale write ends from blocking the parent's `read()`.

#### Child Autonomous Behaviour (Milestone 5 requirement)

Each child process reads the graph file **independently**, runs Dijkstra on its own, and travels the computed path autonomously. The parent only consumes the status messages and drives the GUI; it never tells the child which path to take.

---

## Milestone 6 – Node-Access Synchronisation

### Compilation and Run
```bash
make milestone6
./sim <input_file>
```

### Synchronisation Mechanism – POSIX Named Semaphores

#### Requirement

At most **one traveler** may occupy any given node at a time. Each traveler stays inside a node for **exactly 1 second**. Others arriving while the node is occupied must wait outside.

#### Implementation

We use **POSIX named semaphores** (`sem_open` / `sem_wait` / `sem_post` / `sem_close`), one semaphore per graph node, named `/os_sim_node_<N>`.

```c
/* Open (or create) the binary semaphore for node N */
sem_t *s = sem_open("/os_sim_node_N", O_CREAT, 0644, /*initial_value=*/1);

/* ── WAITING OUTSIDE ─────────────────────── */
send_status_to_parent(from, to, SYNC_WAITING, ...);  // notify GUI
sem_wait(s);          /* blocks until node is free; semaphore → 0 */

/* ══ CRITICAL SECTION: inside the node ═══ */
send_status_to_parent(to, next, SYNC_INSIDE, ...);   // notify GUI
usleep(1000 * 1000);  /* stay exactly 1 second      */

/* ── LEAVING ─────────────────────────────── */
send_status_to_parent(to, next, SYNC_MOVING, ...);   // notify GUI
sem_post(s);          /* release; semaphore → 1 */
sem_close(s);
```

#### Why named semaphores?

| Property | Detail |
|---|---|
| **Cross-process visibility** | Named semaphores live in the kernel and are accessible by any process that knows the name – no shared-memory region needed. |
| **Binary semantics** | Initial value of 1 makes `sem_wait` behave like a mutex `lock` and `sem_post` like `unlock`. |
| **No starvation** | The Linux kernel unblocks one waiter per `sem_post` call. Because every traveler eventually exits the node, every waiting traveler is eventually unblocked. |
| **Signal safety** | `sem_wait` returns `EINTR` when interrupted by a signal; the child simply retries, so signal delivery does not break the lock. |

#### State Machine per Traveler (Milestone 6)

```
  [MOVING on edge]
        │  arrive at intermediate node
        ▼
  [SYNC_WAITING] ──── sem_wait() blocks ────►  sem acquired
        │                                             │
        │                                             ▼
        │                                      [SYNC_INSIDE]
        │                                      usleep(1 s)
        │                                             │
        │                                             ▼
        │                                      sem_post()
        └─────────────────────────────────────► [SYNC_MOVING]
```

#### GUI Representation

- **`STATE_WAITING_OUTSIDE`** – traveler is drawn offset from the node centre (queue position) with a distinct colour/icon.
- **`STATE_INSIDE_NODE`** – traveler is drawn at the node centre with a bright highlight.
- **`STATE_MOVING_ON_EDGE`** – traveler is drawn along the edge between nodes.

#### Self-Check Criteria

| Criterion | How it is guaranteed |
|---|---|
| No two travelers in the same node simultaneously | The semaphore value is at most 1; `sem_wait` blocks every other process until `sem_post`. |
| No starvation | `sem_post` always unblocks one waiter; every traveler spends at most 1 s in the node, so the queue drains in finite time. |
| GUI reflects waiting state | `SYNC_WAITING` message is sent **before** `sem_wait` blocks, so the parent can show the waiting traveler immediately. |

---

## Second Submission Checklist (Milestone 6, 5%)

- [x] All milestones 1–6 compile and run with `make milestone<N>` → `./sim <file>`
- [x] GitHub tag: `milestone6`
- [x] Video (30–60 s): three travelers waiting for the same node, entering one-by-one, each staying 1 second
- [x] README documents IPC (pipes) and synchronisation (POSIX named semaphores) mechanisms
=======
Child Processes: Each child process represents a single traveler. Upon creation via fork(), the child prints its status ([PID] started) and immediately exits, delegating movement management back to the parent configuration.

6   Synchronisation                       milestone6


MILESTONE 6 - SYNCHRONISATION

In this milestone, every node in the graph acts as a critical section.
Only one traveler process can occupy a node at a time for a full second.
Travelers that arrive at a busy node wait outside in a queue without
causing deadlocks, race conditions, or starvation.
Synchronisation is implemented using POSIX named semaphores (sem_open).


NEW FILES ADDED IN MILESTONE 6

  heavy_traffic.txt  Stress-test graph: 4 travelers race to bottleneck node 4   Shira Kraft
  tester.h           Validation hook declarations (log entry/exit/verify)        Shira Kraft
  tester_m6.c        Validation hook implementations, lock-integrity checker     Shira Kraft
  tester.sh          Automated bash test harness - 7 test suites                 Shira Kraft
  gui_m6.c           Milestone 6 GUI: waiting queue, inside-node rendering       Aviya Ben David
  gui_m6.h           Milestone 6 GUI header, TravelerVisualState enum            Aviya Ben David
  main_m6.c          Milestone 6 entry point, IPC sync-state polling             Aviya Ben David


TRAVELER VISUAL STATES (MILESTONE 6)

  STATE_MOVING_ON_EDGE    Traveler moves along edge - normal colored circle
  STATE_WAITING_OUTSIDE   Traveler blocked outside locked node - orange fill, WAIT label
  STATE_INSIDE_NODE       Traveler holds the lock, inside node - pulsing green ring


BUILD AND RUN - MILESTONE 6

  make clean
  make milestone6
  ./sim heavy_traffic.txt


RUNNING THE TEST HARNESS

Run all 7 test suites:
  chmod +x tester.sh
  ./tester.sh

Run a single suite:
  ./tester.sh --only compile      (suite 1: compilation, zero warnings)
  ./tester.sh --only sanity       (suite 2: normal / disconnected / full graphs)
  ./tester.sh --only deadlock     (suite 3: deadlock and starvation detection)
  ./tester.sh --only error        (suite 4: missing file, no args, bad input)
  ./tester.sh --only integrity    (suite 5: node-access log overlap check)
  ./tester.sh --only valgrind     (suite 6: memory leaks, semaphore cleanup)
  ./tester.sh --only repeat       (suite 7: heavy_traffic.txt x3 repeatability)

Skip Valgrind for faster runs:
  ./tester.sh --skip-valgrind

Test output:
  [PASS]  check succeeded
  [FAIL]  check failed - details printed below the line
  [INFO]  informational message, no pass/fail impact


LOCK INTEGRITY VALIDATION

To enable post-run log analysis, add the hooks to milestone5.c:

  1. Add at top of milestone5.c:
       #include "tester.h"

  2. Immediately after sem_wait(...):
       log_node_entry(child_index, target_node, get_timestamp());

  3. Immediately before sem_post(...):
       log_node_exit(child_index, target_node, get_timestamp());

  4. Build with hooks enabled:
       gcc ... tester_m6.c ... -o sim

  5. Run and verify:
       ./sim heavy_traffic.txt
       ./tester.sh --only integrity

  The log is written to: /tmp/m6_node_access.log
  The verifier checks that no two entry-to-exit intervals for the same
  node ever overlap. Any overlap is a synchronisation violation.

  To disable hooks in the submission build without removing the calls:
       gcc -DTESTER_DISABLED ...


GIT TAG - MILESTONE 6 SUBMISSION

  git add .
  git commit -m "Milestone 6: node-access synchronisation + QA harness"
  git tag -a milestone6 -m "Milestone 6 submission"
  git push origin HEAD
  git push origin milestone6

Verify the tag was pushed:
  git ls-remote --tags origin | grep milestone6


DEMO VIDEO - MILESTONE 6

  demo_milestone6.mp4 - screen recording showing:
    1. Building with: make milestone6
    2. Loading heavy_traffic.txt (4 travelers, bottleneck at node 4)
    3. All travelers moving toward node 4 - STATE_MOVING_ON_EDGE
    4. One traveler inside node 4 with pulsing green ring - STATE_INSIDE_NODE
       Three travelers queued outside with orange fill and WAIT label
    5. Travelers entering node 4 one by one in queue order
    6. All travelers reaching their destinations, window closing cleanly
    7. Terminal showing exit code 0:  echo $?

  (Place the recording file in the repository root.)
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3
