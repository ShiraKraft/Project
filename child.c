#include "child.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Global flag – set by the signal handler, checked in the travel loop */
/* ------------------------------------------------------------------ */
static volatile sig_atomic_t g_terminate = 0;

/* ------------------------------------------------------------------ */
/*  Signal handler                                                      */
/*  The parent sends SIGUSR1 when the traveler's journey is complete.  */
/* ------------------------------------------------------------------ */
static void handle_termination(int sig)
{
    (void)sig;          /* suppress unused-parameter warning */
    g_terminate = 1;
}

/* ------------------------------------------------------------------ */
/*  run_child                                                           */
/*                                                                      */
/*  Called immediately after fork() returns 0 (i.e. we are the child). */
/*  path[]  – array of node indices computed by the parent             */
/*  path_len – number of nodes in path                                 */
/*  weights[] – edge weights  weights[i] = weight of edge path[i]→    */
/*              path[i+1];  length = path_len - 1                      */
/* ------------------------------------------------------------------ */
void run_child(const int *path, int path_len, const int *weights)
{
    /* 1. Register signal handler BEFORE printing "started"
          so we never miss a signal sent immediately after fork.       */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                /* do NOT use SA_RESTART so that   */
                                    /* usleep() is interrupted on signal */
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* 2. Announce that this child is alive.
          Format required by the spec: "[PID] started"                 */
    printf("[%d] started\n", (int)getpid());
    fflush(stdout);

    /* 3. Simulate travel.
          For each edge path[i] → path[i+1]:
            • The edge has weight W.
            • W equal-length "hops", each hop = 300 ms (spec §milestone 3).
            • After arriving at an intermediate node, wait 1 full second
              (spec §milestone 3, "wait 1 second at every node except
               source and destination").
          The child does NOT draw anything; the parent handles the GUI.
          We still track time here so the child "lives" for exactly the
          right duration, and exits only when the parent signals it.   */

    for (int i = 0; i < path_len - 1 && !g_terminate; i++) {
        int W = weights[i];

        /* Travel across the edge: W hops × 300 ms each */
        for (int hop = 0; hop < W && !g_terminate; hop++) {
            usleep(300 * 1000);     /* 300 000 µs = 300 ms             */
        }

        /* Arrived at an intermediate node – wait 1 second.
           Skip the wait if this is the destination node.              */
        int is_destination = (i == path_len - 2);
        if (!is_destination && !g_terminate) {
            usleep(1000 * 1000);    /* 1 000 000 µs = 1 s              */
        }
    }

    /* 4. Journey is over from the child's perspective.
          Now simply sleep until the parent sends SIGUSR1.
          pause() suspends until ANY signal arrives, so if g_terminate
          is already set we skip straight to cleanup.                  */
    while (!g_terminate) {
        pause();
    }

    /* 5. Clean exit – free nothing here because the child inherited
          the parent's heap but should not double-free shared data.
          The caller (main / fork site) is responsible for freeing
          path and weights before calling run_child in the parent copy.*/
    exit(EXIT_SUCCESS);
}
