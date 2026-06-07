/*
 * milestone5.c – Milestone 5: Autonomous Child Process Logic
 *
 * Each child:
 *   1. Reads the graph from the input file by itself
 *   2. Computes its own Dijkstra path (NO data received from parent)
 *   3. "Travels" node by node with correct timing delays
 *   4. Sends a StatusMessage to the parent on every node arrival
 *   5. Sends a final "finished" message and exits cleanly
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   /* usleep, getpid */

/* ── Timing constants (match milestone 3 spec) ─────────────────────────── */
#define HOP_DELAY_US    (300 * 1000)   /* 300 ms per hop on an edge       */
#define NODE_WAIT_US    (1000 * 1000)  /* 1 s wait at intermediate nodes  */

/* ── Helper: edge weight lookup in CompactGraph ────────────────────────── */
/*
 * Returns the weight of the directed edge u→v, or -1 if not found.
 * Used to compute how long the child should sleep while crossing each edge.
 */
static int edge_weight(const CompactGraph *g, int u, int v)
{
    for (int e = g->head[u]; e != -1; e = g->edges[e].next) {
        if (g->edges[e].dest == v)
            return g->edges[e].weight;
    }
    return -1; /* should never happen on a valid Dijkstra path */
}

/* ══════════════════════════════════════════════════════════════════════════
 * run_child_m5
 *
 * Entry point called in the child branch (pid == 0) after fork().
 *
 * Parameters:
 *   filename     – path to the input file (graph + travelers section)
 *   src          – source node index for this traveler
 *   dst          – destination node index for this traveler
 *   child_index  – 0-based index of this child (used for pipe selection)
 *   total        – total number of children (used to close other pipes)
 *
 * IMPORTANT: The parent must NOT pass the computed path to this function.
 *            The child reads the graph file and runs Dijkstra itself.
 * ══════════════════════════════════════════════════════════════════════════ */
void run_child_m5(int src, int dst,
                  const char *filename,
                  int child_index, int total_children)
{
    /* ── Step 1: Close all pipe ends we don't own ──────────────────────── */
    /*
     * child_ipc_set_total_children() was already called in the parent
     * before fork(), so the value is already set. We just call init here.
     */
    (void)total_children; /* value was set via child_ipc_set_total_children() */
    if (init_child_ipc_side(child_index) != 0) {
        exit(EXIT_FAILURE);
    }

    /* ── Step 2: Read the graph from the file (autonomous – no parent data) */
    /*
     * We build a CompactGraph because run_dijkstra() uses it.
     * We parse manually: skip the "# travelers" section at the end.
     */
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

    /* ── Step 3: Run Dijkstra locally ──────────────────────────────────── */
    PathResult result = run_dijkstra(&g, src, dst);

    if (!result.found) {
        /*
         * No path exists. Send a single "arrived at src, destination=true"
         * message so the parent knows this child is done, then exit.
         */
        send_status_to_parent(src, -1, true, false);
        send_status_to_parent(src, -1, true, true);
        close_child_ipc_side();
        exit(EXIT_SUCCESS);
    }

    /* ── Step 4: Travel node-by-node, sending a status on each arrival ─── */
    for (int i = 0; i < result.length; i++) {
        int current   = result.path[i];
        bool is_dest  = (i == result.length - 1);
        int  next     = is_dest ? -1 : result.path[i + 1];

        /* Notify the parent we arrived at this node */
        send_status_to_parent(current, next, is_dest, false);

        if (!is_dest) {
            /* Cross the edge to the next node:
             *   weight W  →  W hops × 300 ms each  */
            int w = edge_weight(&g, current, next);
            if (w < 1) w = 1; /* safety guard */

            for (int hop = 0; hop < w; hop++) {
                usleep(HOP_DELAY_US);
            }

            /* Wait 1 second at every intermediate node
             * (spec: not at source, not at destination)           */
            if (i > 0) {          /* i==0 is the source – skip wait */
                usleep(NODE_WAIT_US);
            }
        }
    }

    /* ── Step 5: Send "finished" and exit cleanly ──────────────────────── */
    send_status_to_parent(-1, -1, false, true);
    close_child_ipc_side();
    exit(EXIT_SUCCESS);
}