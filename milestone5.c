/*
 * milestone5.c – Milestone 6: Autonomous Child Process with Node Synchronisation
 *
 * Each child process:
 * 1. Reads the graph file independently and runs Dijkstra to find its path.
 * 2. Travels hop-by-hop (300 ms per hop unit).
 * 3. Before entering each intermediate or destination node, it acquires a 
 * per-node POSIX named semaphore ("/os_sim_node_<index>") – the critical section.
 * 4. Stays inside the node for exactly 1 second, then releases the semaphore.
 * 5. Reports state transitions using send_status_m6() so that every message 
 * carries the correct sync_state and target_node fields.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "milestone5.h"
#include "child_ipc.h"       /* send_status_m6, init_child_ipc_side, close_child_ipc_side */
#include "ipc_protocol.h"    /* SyncState constants (SYNC_WAITING, SYNC_INSIDE, SYNC_MOVING) */
#include "graph.h"           /* CompactGraph, init_graph, add_edge */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>           /* O_CREAT */
#include <sys/stat.h>        /* Mode bits for sem_open */


double get_timestamp(void);
void log_node_entry(int traveler_id, int node_id, double timestamp);
void log_node_exit(int traveler_id, int node_id, double timestamp);


/* ── Timing constants ───────────────────────────────────────────── */
#define HOP_DELAY_US   (500 * 1000)    /* 300 ms per edge hop        */
#define NODE_WAIT_US  (3000 * 1000)    /* 1 s dwell at each node     */

/* ── Helpers for Per-Node Semaphores ────────────────────────────── */

/* Build a unique semaphore name for a specific node index */
static void sem_name_for_node(int node, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "/os_sim_node_%d", node);
}

/* Open or create a unique semaphore for a specific node */
static sem_t *open_node_sem(int node)
{
    char name[64];
    sem_name_for_node(node, name, sizeof(name));

    /* Open with O_CREAT so multiple children can safely attach to the same node semaphore */
    sem_t *s = sem_open(name, O_CREAT, 0644, 1);
    if (s == SEM_FAILED) {
        perror("[child_m6] sem_open");
        exit(EXIT_FAILURE);
    }
    return s;
}

/* Close the local semaphore handle */
static void close_node_sem(sem_t *s)
{
    if (sem_close(s) == -1)
        perror("[child_m6] sem_close");
}

/* ── Helper: look up the weight of edge u→v ─────────────────────── */
static int edge_weight(const CompactGraph *g, int u, int v)
{
    for (int e = g->head[u]; e != -1; e = g->edges[e].next) {
        if (g->edges[e].dest == v)
            return g->edges[e].weight;
    }
    return 1; /* safety fallback */
}

/* ════════════════════════════════════════════════════════════════════
 * run_child_m5
 * ════════════════════════════════════════════════════════════════════ */
void run_child_m5(int src, int dst,
                  const char *filename,
                  int child_index, int total_children)
{
    (void)total_children;

    /* ── 0. Barrier: all children wait 1.5 s so that every process   ──
     *    is fully forked before anyone starts moving.  This guarantees
     *    they arrive at the bottleneck node simultaneously, making the
     *    FCFS vs SJF scheduling difference clearly visible.           */
    usleep(1500 * 1000);   /* 1.5 s startup barrier */

    /* ── 1. Initialise IPC ─────────────────────────────────────────── */
    if (init_child_ipc_side(child_index) != 0)
        exit(EXIT_FAILURE);

    /* ── 2. Read graph from file independently ──────────────────────── */
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("[child] fopen");
        close_child_ipc_side();
        exit(EXIT_FAILURE);
    }

    CompactGraph g;
    int N, M;
    if (fscanf(f, "%d %d", &N, &M) != 2) {
        fclose(f); 
        close_child_ipc_side(); 
        exit(EXIT_FAILURE);
    }
    init_graph(&g, N);

    for (int i = 0; i < M; i++) {
        int u, v, w;
        if (fscanf(f, "%d %d %d", &u, &v, &w) != 3) {
            fclose(f); 
            close_child_ipc_side(); 
            exit(EXIT_FAILURE);
        }
        add_edge(&g, u, v, w);
    }
    fclose(f);

    /* ── 3. Run Dijkstra ───────────────────────────────────────────── */
    PathResult result = run_dijkstra(&g, src, dst);

    if (!result.found) {
        /* No path exists: notify parent and exit safely */
        send_status_m6(src, -1, true, false, SYNC_MOVING, src);
        send_status_m6(src, -1, true, true,  SYNC_MOVING, src);
        close_child_ipc_side();
        exit(EXIT_SUCCESS);
    }

    /* ════════════════════════════════════════════════════════════
     * Travel loop
     * ════════════════════════════════════════════════════════════ */
    for (int i = 0; i < result.length; i++) {
        int  cur    = result.path[i];
        bool is_dst = (i == result.length - 1);
        int  nxt    = is_dst ? -1 : result.path[i + 1];

        if (i == 0) {
            /*
             * Source node: No lock needed to start.
             * Notify parent we are moving toward the next node, then cross edge.
             */
            send_status_m6(cur, nxt, false, false, SYNC_MOVING, nxt);

            int w = edge_weight(&g, cur, nxt);
            for (int h = 0; h < w; h++)
                usleep(HOP_DELAY_US);

        } else if (!is_dst) {
            /*
             * Intermediate node:
             * 1. Notify WAITING – queued outside this specific node
             * 2. Acquire per-node lock – blocks if another process is inside
             * 3. Notify INSIDE  – entered the node
             * 4. Dwell 1 second – critical section
             * 5. Release per-node lock
             * 6. Notify MOVING  – leaving toward next node
             * 7. Cross the edge
             */
            send_status_m6(cur, nxt, false, false, SYNC_WAITING, cur);  /* 1 */
            
            sem_t *node_sem = open_node_sem(cur);
            while (sem_wait(node_sem) == -1) {
                if (errno != EINTR) {
                    perror("[child_m6] sem_wait");
                    close_node_sem(node_sem);
                    close_child_ipc_side();
                    exit(EXIT_FAILURE);
                }
            }                                                           /* 2 */

            log_node_entry(child_index, cur, get_timestamp());
            
            send_status_m6(cur, nxt, false, false, SYNC_INSIDE, cur);   /* 3 */
            usleep(NODE_WAIT_US);                                       /* 4 */
            
            sem_post(node_sem);                                         /* 5 */
            log_node_exit(child_index, cur, get_timestamp());
            close_node_sem(node_sem);

            send_status_m6(cur, nxt, false, false, SYNC_MOVING, nxt);   /* 6 */

            int w = edge_weight(&g, cur, nxt);                          /* 7 */
            for (int h = 0; h < w; h++)
                usleep(HOP_DELAY_US);

        } else {
            /*
             * Destination node:
             * 1. Notify WAITING
             * 2. Acquire per-node lock
             * 3. Notify INSIDE (is_destination = true)
             * 4. Dwell 1 second
             * 5. Release per-node lock
             */
            send_status_m6(cur, -1, true, false, SYNC_WAITING, cur);   /* 1 */
            
            sem_t *node_sem = open_node_sem(cur);
            while (sem_wait(node_sem) == -1) {
                if (errno != EINTR) {
                    perror("[child_m6] sem_wait");
                    close_node_sem(node_sem);
                    close_child_ipc_side();
                    exit(EXIT_FAILURE);
                }
            }                                                          /* 2 */

            send_status_m6(cur, -1, true, false, SYNC_INSIDE, cur);    /* 3 */
            usleep(NODE_WAIT_US);                                      /* 4 */
            
            sem_post(node_sem);                                        /* 5 */
            log_node_exit(child_index, cur, get_timestamp());
            close_node_sem(node_sem);
        }
    }

    /* ── 4. Journey complete: send finished message ──────────────── */
    send_status_m6(-1, -1, false, true, SYNC_MOVING, -1);
    close_child_ipc_side();
    
    exit(EXIT_SUCCESS);
}