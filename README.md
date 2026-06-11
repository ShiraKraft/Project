OS Project - Traffic Simulation on a Directed Graph
Semester project for the Operating Systems course


TEAM MEMBERS AND RESPONSIBILITIES

Shira Kraft - Algorithms / Path Logic, QA & Documentation
  Dijkstra implementation, path reconstruction, edge cases,
  test suite (tester.c, test.sh), input validation, README
  Files: Dijkstra.c, gui.h, gui.c, tester.c, test.sh

Bat-El Zairi - Graph Setup & Data Management
  Graph structure, file parser, memory management
  Files: graph.c, graph.h

Aviya Ben David - System, GUI & Quality Assurance
  Raylib GUI, terminal output formatting, Valgrind memory checks
  Files: gui.c, gui.h, main.c

Hila - Synchronisation Architecture & Locking Mechanisms
  Semaphore-based critical sections, ensuring only one traveler
  occupies a node at a time, deadlock and starvation prevention
  Files: milestone5.c, gui_m6.c, gui_m6.h, main_m6.c


PROJECT DESCRIPTION

This project implements a directed weighted graph simulation in C.
The system finds the shortest path between nodes using Dijkstra's algorithm
and visualises the graph using the raylib library.

The graph represents a city traffic system where each node is an intersection
and each directed edge is a road whose weight represents travel time in minutes.


TECH STACK

  Language:         C
  Environment:      Linux
  GUI Library:      raylib
  Version Control:  GitHub



FILE STRUCTURE

  main.c           Entry point, argument parsing, run, cleanup       Aviya Ben David
  graph.c          Adjacency-list graph, file parser, Dijkstra        Bat-El Zairi
  graph.h          Unified header, structs, constants, declarations   Bat-El Zairi
  Dijkstra.c       Compact-graph Dijkstra, path reconstruction        Shira Kraft
  gui.c            Raylib GUI, terminal formatting, QA, system tests  Aviya Ben David
  gui.h            GUI declarations                                   Aviya Ben David
  tester.c         Automated test suite - 12 tests                    Shira Kraft
  Makefile         Build configuration
  CMakeLists.txt   CMake build configuration


HOW TO BUILD AND RUN

Build all milestones:
  make

Build a specific milestone:
  make milestone1   (terminal Dijkstra only)
  make milestone2   (static GUI)
  make milestone3   (animated GUI - 300 ms/step, 1 s node pause)

Run - Milestone 1 (terminal):
  ./dijkstra <input_file>

Run - Milestones 2 & 3 (GUI):
  ./sim <input_file>
  Press the Play button in the GUI window to start the animation.


INPUT FILE FORMAT

  N M
  src1 dst1 weight1
  src2 dst2 weight2
  ...
  query_src query_dst

Example input:
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


ANIMATION TIMING (MILESTONE 3)

  Step delay along edge:   300 ms   (macro: STEP_DELAY_MS)
  Node arrival pause:     1000 ms   (macro: NODE_WAIT_MS)

Both values are injected at compile time via -D flags in the milestone3 Makefile
target and consumed in gui.c. To override without editing the Makefile:
  make milestone3 STEP_DELAY_MS=500 NODE_WAIT_MS=2000


RUNNING TESTS

  gcc -o tester tester.c graph.c -lm
  ./tester ./dijkstra

Or simply:
  make test


DEMO VIDEO

  demo_milestone3.mp4 - screen recording showing:
    1. Building with: make milestone3
    2. Loading the example 6-node graph
    3. Pressing Play - the traveller moves along the shortest path
       (0 -> 2 -> 1 -> 3 -> 4 -> 5, cost 11) with 300 ms step delay
       and 1-second pause at each node highlighted in the GUI.

  (Place the recording file in the repository root and update this path if needed.)


MILESTONES

  1   Graph representation and algorithms   milestone1
  2   GUI, graph visualisation              milestone2
  3   Movement animation on graph           milestone3
  4   Multiple processes and parent         milestone4
  5   Inter-process communication           milestone5
  6   Synchronisation                       milestone6
  7   Scheduling algorithms                 milestone7
  ## Milestone 4 Extension

### Compilation Instructions
To compile the project for Milestone 4, run the following command in the terminal:
```bash
make milestone4
./sim imput.txt
```
Design Overview
In this milestone, the simulation transitions to a multi-process architecture.

Parent Process: Parses the extended input file, calculates paths for all travelers using Dijkstra's algorithm, forks child processes, and runs the Raylib GUI loop to render all travelers simultaneously in different colors. Before exiting, the parent properly reaps all terminated children to prevent zombie processes.

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