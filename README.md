OS Project - Traffic Simulation on a Directed Graph
Semester project for the Operating Systems course

Team Members and Responsibilities

Shira Kraft
  - QA & Algorithms / Path Logic
  - Dijkstra implementation, path reconstruction, edge cases, test suite
  - Files: Dijkstra.c, tester.c

Bat-El Zairi
  - Graph Setup & Data Management
  - Graph structure, file parser, input validation, memory management
  - Files: graph.c, graph.h

Aviya Ben David
  - System, GUI & Quality Assurance
  - Raylib GUI, terminal output formatting, Valgrind memory checks
  - Files: gui.c, gui.h, main.c


Project Description

This project implements a directed weighted graph simulation in C.
The system finds the shortest path between nodes using Dijkstra's algorithm
and visualizes the graph using the raylib library.

The graph represents a city traffic system where each node is an intersection
and each directed edge is a road with a weight representing travel time in minutes.


Tech Stack

  Language:    C
  Environment: Linux
  GUI Library: raylib
  Version Control: GitHub


File Structure

  main.c         - Entry point, argument parsing, run, cleanup         (Aviya Ben David)
  graph.c        - Adjacency-list graph, file parser, dijkstra          (Bat-El Zairi)
  graph.h        - Unified header, structs, constants, declarations     (Bat-El Zairi)
  Dijkstra.c     - Compact-graph Dijkstra, path reconstruction          (Shira Kraft)
  gui.c          - Raylib GUI, terminal formatting, QA, system tests    (Aviya Ben David)
  gui.h          - GUI declarations                                     (Aviya Ben David)
  tester.c       - Automated test suite, 12 tests                       (Shira Kraft)
  Makefile       - Build configuration
  CMakeLists.txt - CMake build configuration


How to Build and Run

  Build:
    make

  Run:
    ./Project <input_file>

  Input file format:
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


Running Tests

  gcc -o tester tester.c graph.c -lm
  ./tester ./Project


Milestones

  1 - Graph representation and algorithms       (milestone1)
  2 - GUI, graph visualization                  (milestone2)
  3 - Movement animation on graph               (milestone3)
  4 - Multiple processes and parent process     (milestone4)
  5 - Inter-process communication               (milestone5)
  6 - Synchronization                           (milestone6)
  7 - Scheduling algorithms                     (milestone7)
