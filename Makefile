# הגדרת הקומפיילר והדגלים
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LIBS = -lraylib -lGL -lX11 -lm -lpthread

# קובצי המקור לסימולציה הגרפית בלבד (בלי tester.c!)
SRCS = main.c child.c graph.c Dijkstra.c gui.c milestone5.c child_ipc.c parent_ipc.c gui_parent.c milestone4.c
OBJS = $(SRCS:.c=.o)

# יעד ברירת המחדל
all: sim

milestone4: sim
milestone5: sim

# יצירת קובץ ההרצה - ודאי שיש כאן Tab בתחילת השורה
sim: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o sim $(LIBS)

# חוק קימפול כללי - ודאי שיש כאן Tab בתחילת השורה
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ניקוי קבצים - ודאי שיש כאן Tab בתחילת השורה
clean:
	rm -f sim *.o

.PHONY: all clean milestone4 milestone5