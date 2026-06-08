/*
 * milestone5.c – Milestone 5: Autonomous Child Process Logic
 *
 * Each child:
 * 1. Reads the graph from the input file by itself
 * 2. Computes its own Dijkstra path (NO data received from parent)
 * 3. "Travels" node by node with correct timing delays
 * 4. Sends a StatusMessage to the parent on every node arrival
 * 5. Sends a final "finished" message and exits cleanly
 *
 * The parent is responsible for all GUI updates and log printing.
 * The child never prints anything to the terminal.
 *
 * IPC mechanism: one pipe per child (defined in parent_ipc / child_ipc).
 * Transport format: fixed-size StatusMessage structs (see ipc_protocol.h).
 */

#define _POSIX_C_SOURCE 200809L

#include "milestone5.h"
#include "child_ipc.h"
#include "graph.h"
#include <semaphore.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   /* usleep, getpid */

/* ── Timing constants (match milestone 3 spec) ─────────────────────────── */
#define HOP_DELAY_US    (300 * 1000)   /* 300 ms per hop on an edge         */
#define NODE_WAIT_US    (1000 * 1000)  /* 1 s wait at intermediate nodes  */

/* ── Helper: edge weight lookup in CompactGraph ────────────────────────── */
static int edge_weight(const CompactGraph *g, int u, int v)
{
    for (int e = g->head[u]; e != -1; e = g->edges[e].next) {
        if (g->edges[e].dest == v)
            return g->edges[e].weight;
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * run_child_m5
 * ══════════════════════════════════════════════════════════════════════════ */
void run_child_m5(int src, int dst,
                  const char *filename,
                  int child_index, int total_children)
{
    (void)total_children;
    if (init_child_ipc_side(child_index) != 0) {
        exit(EXIT_FAILURE);
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("[child] fopen");
        close_child_ipc_side();
        exit(EXIT_FAILURE);
    }

    CompactGraph g;
    int N, M;
    if (fscanf(f, "%d %d", &N, &M) != 2) {
        fprintf(stderr, "[child] bad file format\n");
        fclose(f);
        close_child_ipc_side();
        exit(EXIT_FAILURE);
    }
    init_graph(&g, N);

    for (int i = 0; i < M; i++) {
        int u, v, w;
        if (fscanf(f, "%d %d %d", &u, &v, &w) != 3) {
            fprintf(stderr, "[child] bad edge at line %d\n", i + 2);
            fclose(f);
            close_child_ipc_side();
            exit(EXIT_FAILURE);
        }
        add_edge(&g, u, v, w);
    }
    fclose(f);

    PathResult result = run_dijkstra(&g, src, dst);

    if (!result.found) {
        send_status_to_parent(src, -1, true, false);
        send_status_to_parent(src, -1, true, true);
        close_child_ipc_side();
        exit(EXIT_SUCCESS);
    }

    /* ── Step 4: Travel node-by-node ─── */
    sem_t *sem = sem_open("/node_lock", O_CREAT, 0644, 1);
    for (int i = 0; i < result.length; i++) {
        int current   = result.path[i];
        bool is_dest  = (i == result.length - 1);
        int  next     = is_dest ? -1 : result.path[i + 1];

        send_status_to_parent(current, next, is_dest, false);

        if (!is_dest) {
            sem_wait(sem);
            int w = edge_weight(&g, current, next);
            if (w < 1) w = 1;

            for (int hop = 0; hop < w; hop++) {
                usleep(HOP_DELAY_US);
            }

            if (i > 0) {
                usleep(NODE_WAIT_US);
            }
            sem_post(sem);
        }
    }
    sem_close(sem);

    send_status_to_parent(-1, -1, false, true);
    close_child_ipc_side();
    exit(EXIT_SUCCESS);
}