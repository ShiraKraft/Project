# =============================================================================
# Makefile  –  Network Traffic Simulation (Milestones 1 – 7)
#
# Usage
# ─────
#   make milestone1    →  dijkstra          (terminal Dijkstra, M1)
#   make milestone2    →  sim               (static GUI, M2)
#   make milestone3    →  sim               (cyber animation, M3 – same binary)
#   make milestone4    →  sim-m4            (forked travelers + IPC, M4)
#   make milestone5    →  sim-m5            (edge animation + IPC, M5)
#   make milestone6    →  sim-m6            (node sync + waiting queue, M6)
#   make milestone7    →  sim-schd          (FCFS/SJF scheduling + GUI, M7)
#   make all           →  builds every milestone target
#   make clean         →  removes all binaries and object files (no errors)
#
# Run examples
# ────────────
#   ./dijkstra input.txt
#   ./sim      input.txt
#   ./sim-m4   input.txt
#   ./sim-m5   input.txt
#   ./sim-m6   input.txt
#   ./sim-schd fcfs heavy_traffic.txt
#   ./sim-schd sjf  heavy_traffic.txt
# =============================================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L
LIBS    = -lm -lpthread

# Raylib flags (static lib bundled in repo root)
RAYLIB  = -I. -L. -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# ── Source groups ──────────────────────────────────────────────────────────

# Shared graph / Dijkstra kernel (used by every milestone)
GRAPH_SRC = graph.c Dijkstra.c

# Milestones 1-3: terminal + basic GUI + cyber animation
M1_SRC    = main.c $(GRAPH_SRC) gui.c
M2_SRC    = $(M1_SRC)
M3_SRC    = $(M1_SRC)

# Milestone 4: forked traveler processes (no Raylib window yet)
M4_SRC    = main_m4.c $(GRAPH_SRC) milestone4.c gui.c

# Milestone 5: full Raylib simulation with moving travelers
M5_SRC    = main_m5.c $(GRAPH_SRC) milestone5.c gui_parent.c \
            parent_ipc.c child_ipc.c signal_handler.c

# Milestone 6: node-access synchronisation + waiting queue
M6_SRC    = main_m6.c $(GRAPH_SRC) milestone5.c \
            parent_ipc.c child_ipc.c gui_parent.c gui_m6.c \
            signal_handler.c waiting_queue.c tester.c

# Milestone 7: FCFS / SJF scheduler + GUI overlay
# tester.c provides: get_timestamp, log_node_entry, log_node_exit
# (declared in milestone5.c but defined in tester.c)
M7_SRC    = main_m7.c $(GRAPH_SRC) milestone5.c \
            parent_ipc.c child_ipc.c gui_parent.c gui_m6.c gui_m6_m7.c \
            signal_handler.c waiting_queue.c scheduler.c tester.c

# ── Top-level targets ──────────────────────────────────────────────────────

.PHONY: all milestone1 milestone2 milestone3 milestone4 \
        milestone5 milestone6 milestone7 clean

all: milestone1 milestone2 milestone4 milestone5 milestone6 milestone7

# Milestone 1 – terminal-only Dijkstra
milestone1: dijkstra

dijkstra: $(M1_SRC)
	$(CC) $(CFLAGS) -o $@ $(M1_SRC) $(LIBS) $(RAYLIB)

# Milestones 2 & 3 share the same binary (./sim)
milestone2 milestone3: sim

sim: $(M2_SRC)
	$(CC) $(CFLAGS) -o $@ $(M2_SRC) $(LIBS) $(RAYLIB)

# Milestone 4
milestone4: sim-m4

sim-m4: $(M4_SRC)
	$(CC) $(CFLAGS) -o $@ $(M4_SRC) $(LIBS) $(RAYLIB)

# Milestone 5
milestone5: sim-m5

sim-m5: $(M5_SRC)
	$(CC) $(CFLAGS) -o $@ $(M5_SRC) $(LIBS) $(RAYLIB)

# Milestone 6
milestone6: sim-m6

sim-m6: $(M6_SRC)
	$(CC) $(CFLAGS) -o $@ $(M6_SRC) $(LIBS) $(RAYLIB)

# Milestone 7  (FCFS / SJF scheduling)
milestone7: sim-schd

sim-schd: $(M7_SRC)
	$(CC) $(CFLAGS) -o $@ $(M7_SRC) $(LIBS) $(RAYLIB)

# ── Clean ──────────────────────────────────────────────────────────────────
# Uses $(wildcard ...) so the rule never fails on a fresh checkout where
# no binaries exist yet (avoids "No such file or directory" errors).

clean:
	$(RM) dijkstra sim sim-m4 sim-m5 sim-m6 sim-schd
	$(RM) $(wildcard *.o)
	@echo "[clean] workspace tidy"