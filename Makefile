
# ============================================================
# Makefile – OS Project (Milestones 1–6)
# ============================================================
 
CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TFLAGS = -Wall -Wextra -g
LIBS   = -lm -lpthread
RAYLIB_FLAGS = -I. -L. \
	-lraylib -lGL -lm -lpthread -ldl -lrt -lX11
 
# ── Source groups ──────────────────────────────────────────
DIJKSTRA_SRC = main.c graph.c Dijkstra.c gui.c
SIM_SRC      = main.c graph.c Dijkstra.c gui.c
TESTER_SRC   = tester.c graph.c
 
M4_SRC = main_m4.c milestone4.c child.c gui.c graph.c Dijkstra.c
 
M5_SRC = main_m5.c milestone5.c \
         graph.c Dijkstra.c \
         parent_ipc.c child_ipc.c \
         gui_parent.c signal_handler.c
		 
 M6_SRC = main_m5.c milestone5.c logger.c \
         graph.c Dijkstra.c \
         parent_ipc.c child_ipc.c \
         gui_parent.c signal_handler.c
 
.PHONY: all milestone1 milestone2 milestone3 milestone4 \
        milestone5 milestone6 clean test
 
all: milestone1 milestone2 milestone3
 
# ── Milestone 1 – terminal Dijkstra (./dijkstra <file>) ───
milestone1: $(DIJKSTRA_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -o dijkstra $(DIJKSTRA_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: dijkstra (milestone 1)"
 
# ── Milestone 2 – static GUI (./sim <file>) ────────────────
milestone2: $(SIM_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -o sim $(SIM_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 2)"
 
# ── Milestone 3 – animated GUI (./sim <file>) ──────────────
milestone3: $(SIM_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -DSTEP_DELAY_MS=300 -DNODE_WAIT_MS=1000 \
	      -o sim $(SIM_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 3 – animation enabled)"
 
# ── Milestone 4 – multi-process, no IPC (./sim <file>) ─────
milestone4: $(M4_SRC) milestone4.h gui.h graph.h child.h
	$(CC) $(CFLAGS) -o sim $(M4_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 4)"
 
# ── Milestone 5 – autonomous children + pipe IPC ───────────
milestone5: $(M5_SRC) milestone5.h parent_ipc.h child_ipc.h \
            ipc_protocol.h graph.h gui_parent.h signal_handler.h
	$(CC) $(CFLAGS) -o sim $(M5_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 5 – IPC)"
 
# ── Milestone 6 – node-access synchronisation ──────────────
# Uses POSIX named semaphores (sem_open/sem_wait/sem_post).
# -lrt is already included in RAYLIB_FLAGS; listed explicitly for clarity.
milestone6: $(M6_SRC) milestone5.h parent_ipc.h child_ipc.h \
            ipc_protocol.h graph.h gui_m6.h signal_handler.h
	$(CC) $(CFLAGS) -o sim $(M6_SRC) $(LIBS) $(RAYLIB_FLAGS) -lrt
	@echo "Built: sim (milestone 6 – synchronisation)"
 
# ── Tester ─────────────────────────────────────────────────
tester: $(TESTER_SRC) graph.h
	$(CC) $(TFLAGS) -o tester $(TESTER_SRC) $(LIBS)
 
test: milestone1 tester
	./tester ./dijkstra
 
# ── Clean ──────────────────────────────────────────────────
clean:
	rm -f dijkstra sim tester
	rm -f /tmp/os_test_*.txt
	@echo "Cleaned."