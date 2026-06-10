/*
 * milestone5.c  –  Milestone 6: Autonomous Child Process Logic (Updated)
 *
 * Changes from Milestone 5:
 *   Uses send_status_m6() instead of send_status_to_parent() so that
 *   every message carries the correct sync_state and target_node fields.
 *
 *   Synchronisation flow for every non-source node:
 *     1. send SYNC_WAITING  – traveler is queued outside the node
 *     2. sem_wait(sem)      – block until the node is free
 *     3. send SYNC_INSIDE   – traveler entered the node
 *     4. usleep(1 s)        – mandatory dwell time (critical section)
 *     5. sem_post(sem)      – release the lock
 *
 *   While crossing an edge:
 *     send SYNC_MOVING      – traveler is moving, holds no lock
 */

#define _POSIX_C_SOURCE 200809L

#include "milestone5.h"
#include "child_ipc.h"       /* send_status_m6, init_child_ipc_side */
#include "ipc_protocol.h"    /* SyncState */
#include "graph.h"
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* ── Timing constants ───────────────────────────────────────────── */
#define HOP_DELAY_US   (300 * 1000)    /* 300 ms per edge hop        */
#define NODE_WAIT_US  (1000 * 1000)    /* 1 s dwell at each node     */
#define SEM_NAME       "/node_lock"

/* ── Helper: look up the weight of edge u→v ─────────────────────── */
static int edge_weight(const CompactGraph *g, int u, int v)
{
    for (int e = g->head[u]; e != -1; e = g->edges[e].next)
        if (g->edges[e].dest == v)
            return g->edges[e].weight;
    return 1; /* safety fallback */
}

/* ════════════════════════════════════════════════════════════════════
 *  run_child_m5
 * ════════════════════════════════════════════════════════════════════ */
void run_child_m5(int src, int dst,
                  const char *filename,
                  int child_index, int total_children)
{
    (void)total_children;

    /* ── Initialise IPC ─────────────────────────────────────────── */
    if (init_child_ipc_side(child_index) != 0)
        exit(EXIT_FAILURE);

    /* ── Read graph from file ───────────────────────────────────── */
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("[child] fopen");
        close_child_ipc_side();
        exit(EXIT_FAILURE);
    }

    CompactGraph g;
    int N, M;
    if (fscanf(f, "%d %d", &N, &M) != 2) {
        fclose(f); close_child_ipc_side(); exit(EXIT_FAILURE);
    }
    init_graph(&g, N);

    for (int i = 0; i < M; i++) {
        int u, v, w;
        if (fscanf(f, "%d %d %d", &u, &v, &w) != 3) {
            fclose(f); close_child_ipc_side(); exit(EXIT_FAILURE);
        }
        add_edge(&g, u, v, w);
    }
    fclose(f);

    /* ── Run Dijkstra ───────────────────────────────────────────── */
    PathResult result = run_dijkstra(&g, src, dst);

    if (!result.found) {
        /* No path exists: notify parent and exit */
        send_status_m6(src, -1, true, false, SYNC_MOVING, src);
        send_status_m6(src, -1, true, true,  SYNC_MOVING, src);
        close_child_ipc_side();
        exit(EXIT_SUCCESS);
    }

    /* ── Open named semaphore ───────────────────────────────────── */
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("[child] sem_open");
        close_child_ipc_side();
        exit(EXIT_FAILURE);
    }

    /* ════════════════════════════════════════════════════════════
     *  Travel loop
     *
     *  result.path[0]               = src  (starting node)
     *  result.path[result.length-1] = dst  (destination node)
     *
     *  For each node i in the path:
     *    i == 0   : source – just send MOVING and cross the first edge
     *    0 < i < last : intermediate – WAITING → lock → INSIDE →
     *                   dwell → unlock → MOVING → cross edge
     *    i == last: destination – WAITING → lock → INSIDE →
     *               dwell → unlock
     * ════════════════════════════════════════════════════════════ */
    for (int i = 0; i < result.length; i++) {
        int  cur    = result.path[i];
        bool is_dst = (i == result.length - 1);
        int  nxt    = is_dst ? -1 : result.path[i + 1];

        if (i == 0) {
            /*
             * Source node: no lock needed (traveler starts here).
             * Notify parent we are moving toward the next node,
             * then cross the first edge.
             */
            send_status_m6(cur, nxt, false, false, SYNC_MOVING, nxt);

            int w = edge_weight(&g, cur, nxt);
            for (int h = 0; h < w; h++)
                usleep(HOP_DELAY_US);

        } else if (!is_dst) {
            /*
             * Intermediate node:
             *   1. Notify WAITING – queued outside the node
             *   2. Acquire lock   – blocks if node is occupied
             *   3. Notify INSIDE  – entered the node
             *   4. Dwell 1 second – critical section
             *   5. Release lock
             *   6. Notify MOVING  – leaving toward next node
             *   7. Cross the edge
             */
            send_status_m6(cur, nxt, false, false, SYNC_WAITING, cur);  /* 1 */
            sem_wait(sem);                                               /* 2 */
            send_status_m6(cur, nxt, false, false, SYNC_INSIDE, cur);   /* 3 */
            usleep(NODE_WAIT_US);                                        /* 4 */
            sem_post(sem);                                               /* 5 */
            send_status_m6(cur, nxt, false, false, SYNC_MOVING, nxt);   /* 6 */

            int w = edge_weight(&g, cur, nxt);                          /* 7 */
            for (int h = 0; h < w; h++)
                usleep(HOP_DELAY_US);

        } else {
            /*
             * Destination node:
             *   1. Notify WAITING
             *   2. Acquire lock
             *   3. Notify INSIDE (is_destination = true)
             *   4. Dwell 1 second
             *   5. Release lock
             */
            send_status_m6(cur, -1, true, false, SYNC_WAITING, cur);   /* 1 */
            sem_wait(sem);                                              /* 2 */
            send_status_m6(cur, -1, true, false, SYNC_INSIDE, cur);    /* 3 */
            usleep(NODE_WAIT_US);                                       /* 4 */
            sem_post(sem);                                              /* 5 */
        }
    }

    /* ── Close semaphore and send finished message ──────────────── */
    sem_close(sem);
    send_status_m6(-1, -1, false, true, SYNC_MOVING, -1);
    close_child_ipc_side();
    exit(EXIT_SUCCESS);
}