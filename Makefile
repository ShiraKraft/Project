CC       = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LIBS   = -lm -lpthread
RAYLIB_FLAGS = -I. -L. -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lrt

M7_SRC = main_m6.c milestone5.c graph.c Dijkstra.c parent_ipc.c child_ipc.c gui_parent.c gui_m6.c signal_handler.c waiting_queue.c scheduler.c

milestone7:
	$(CC) $(CFLAGS) -o sim-schd $(M7_SRC) $(LIBS) $(RAYLIB_FLAGS)