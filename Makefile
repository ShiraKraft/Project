# ============================================================
#  Makefile – OS Project (Milestones 1, 2, 3)
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g
TFLAGS   = -Wall -Wextra -g
LIBS     = -lm

RAYLIB_FLAGS = -I. -L. \
               -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

DIJKSTRA_SRC = main.c graph.c Dijkstra.c gui.c
SIM_SRC      = main.c graph.c Dijkstra.c gui.c
TESTER_SRC   = tester.c graph.c
M4_SRC = main_m4.c milestone4.c child.c gui.c graph.c Dijkstra.c
M5_SRC       = main_m4.c milestone4.c child.c gui.c graph.c Dijkstra.c

.PHONY:all milestone1 milestone2 milestone3 milestone4 milestone5 clean test

all: milestone1 milestone2 milestone3

# Milestone 1 – terminal Dijkstra  (./dijkstra <file>)
milestone1: $(DIJKSTRA_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -o dijkstra $(DIJKSTRA_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: dijkstra"

# Milestone 2 – static GUI  (./sim <file>)
milestone2: $(SIM_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -o sim $(SIM_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 2)"

# Milestone 3 – animated cyber GUI  (./sim <file>)
# Same binary as milestone 2; animation activates via the Play button.
milestone3: $(SIM_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -DSTEP_DELAY_MS=300 -DNODE_WAIT_MS=1000 -o sim $(SIM_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: sim (milestone 3 – animation enabled)"

milestone4: $(M4_SRC) milestone4.h gui.h graph.h child.h
	$(CC) $(CFLAGS) -o sim $(M4_SRC) $(LIBS) $(RAYLIB_FLAGS)

	milestone5:
		$(CC) $(CFLAGS) -o sim $(M4_SRC) $(LIBS) $(RAYLIB_FLAGS)
		@echo "Built: sim (milestone 5 - IPC)"

tester: $(TESTER_SRC) graph.h
	$(CC) $(TFLAGS) -o tester $(TESTER_SRC) $(LIBS)

test: milestone1 tester
	./tester ./dijkstra

clean:
	rm -f dijkstra sim tester
	rm -f /tmp/os_test_*.txt
	@echo "Cleaned."