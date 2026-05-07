# ============================================================
#  Makefile – OS Project (Milestone 1 + 2)
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g
# tester uses popen/pclose which need gnu extensions
TFLAGS   = -Wall -Wextra -g
LIBS     = -lm

RAYLIB_FLAGS = -I/usr/local/include -L/usr/local/lib \
               -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

MAIN_SRC    = main.c graph.c
TESTER_SRC  = tester.c graph.c
GUI_SRC     = main.c graph.c gui.c

TARGET      = Project
TESTER_BIN  = tester
GUI_BIN     = ProjectGUI

.PHONY: all clean test valgrind gui

all: $(TARGET) $(TESTER_BIN)

$(TARGET): $(MAIN_SRC) graph.h
	$(CC) $(CFLAGS) -o $@ $(MAIN_SRC) $(LIBS)
	@echo "Built: $(TARGET)"

$(TESTER_BIN): $(TESTER_SRC) graph.h
	$(CC) $(TFLAGS) -o $@ $(TESTER_SRC) $(LIBS)
	@echo "Built: $(TESTER_BIN)"

gui: $(GUI_SRC) graph.h gui.h
	$(CC) $(CFLAGS) -o $(GUI_BIN) $(GUI_SRC) $(LIBS) $(RAYLIB_FLAGS)
	@echo "Built: $(GUI_BIN)"

test: $(TARGET) $(TESTER_BIN)
	@echo ""
	@echo "Running tests..."
	@echo ""
	./$(TESTER_BIN) ./$(TARGET)

valgrind: $(TARGET) $(TESTER_BIN)
	valgrind --leak-check=full --track-origins=yes \
	         ./$(TESTER_BIN) ./$(TARGET)

clean:
	rm -f $(TARGET) $(TESTER_BIN) $(GUI_BIN)
	rm -f /tmp/os_test_*.txt
	@echo "Cleaned."