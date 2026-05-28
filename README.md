OS Project - Traffic Simulation on a Directed Graph
Semester project for the Operating Systems course


TEAM MEMBERS AND RESPONSIBILITIES

Shira Kraft - Algorithms / Path Logic & QA 
  Dijkstra implementation, path reconstruction, edge cases, test suite, input validation,
  Files: Dijkstra.c, gui.h, gui.c, tester.c

Bat-El Zairi - Graph Setup & Data Management
  Graph structure, file parser, memory management
  Files: graph.c, graph.h

Aviya Ben David - System, GUI & Quality Assurance
  Raylib GUI, terminal output formatting, Valgrind memory checks
  Files: gui.c, gui.h, main.c


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