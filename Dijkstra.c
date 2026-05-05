//
// Created by student on 5/5/26.
//
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "Graph.h"

/* ─── Graph helpers ─────────────────────────────────────────────── */

void init_graph(Graph *g, int num_nodes) {
    g->num_nodes  = num_nodes;
    g->edge_count = 0;
    for (int i = 0; i < num_nodes; i++)
        g->head[i] = -1;
}

void add_edge(Graph *g, int u, int v, int weight) {
    /* undirected: add both directions */
    g->edges[g->edge_count] = (Edge){ v, weight, g->head[u] };
    g->head[u] = g->edge_count++;

    g->edges[g->edge_count] = (Edge){ u, weight, g->head[v] };
    g->head[v] = g->edge_count++;
}

/* ─── 1. find_min_distance ──────────────────────────────────────── */
/*
 * Scans dist[] for the unvisited node with the smallest cumulative
 * firewall resistance.
 * Returns -1 if every remaining node is unreachable (termination signal).
 */
int find_min_distance(int dist[], bool visited[], int num_nodes) {
    int min_val = INT_MAX;
    int min_idx = -1;

    for (int i = 0; i < num_nodes; i++) {
        if (!visited[i] && dist[i] < min_val) {
            min_val = dist[i];
            min_idx = i;
        }
    }
    return min_idx;
}

/* ─── 3. reconstruct_path ───────────────────────────────────────── */
/*
 * Traces prev[] backwards from dst to src, then reverses the result
 * to produce a forward step-by-step sequence (e.g. 0 -> 2 -> 5).
 */
void reconstruct_path(int prev[], int dst, int src,
                      int *out_path, int *out_len) {
    int tmp[MAX_NODES];
    int len = 0;
    int cur = dst;

    while (cur != -1) {
        tmp[len++] = cur;
        if (cur == src) break;
        cur = prev[cur];
    }

    /* reverse into output buffer */
    for (int i = 0; i < len; i++)
        out_path[i] = tmp[len - 1 - i];

    *out_len = len;
}

/* ─── 2. run_dijkstra ───────────────────────────────────────────── */
/*
 * Core engine of the cyber simulation.
 *   - Initialises dist[], prev[], visited[]
 *   - Runs the relaxation loop
 *   - Calls reconstruct_path to build the final route
 */
PathResult run_dijkstra(Graph *g, int src, int dst) {
    int  dist[MAX_NODES];
    int  prev[MAX_NODES];
    bool visited[MAX_NODES];
    PathResult result = { .found = false, .length = 0, .total_weight = 0 };

    /* ── Initialisation ──────────────────────────────────────── */
    for (int i = 0; i < g->num_nodes; i++) {
        dist[i]    = INT_MAX;
        prev[i]    = -1;
        visited[i] = false;
    }
    dist[src] = 0;

    /* ── Edge case: source == destination ────────────────────── */
    if (src == dst) {
        result.found        = true;
        result.path[0]      = src;
        result.length       = 1;
        result.total_weight = 0;
        return result;
    }

    /* ── Relaxation loop ─────────────────────────────────────── */
    for (int iter = 0; iter < g->num_nodes; iter++) {

        /* find_min_distance: pick lowest-cost unvisited server */
        int u = find_min_distance(dist, visited, g->num_nodes);

        /* Edge case: disconnected graph / no path exists */
        if (u == -1 || dist[u] == INT_MAX)
            break;

        visited[u] = true;  /* server u is now "compromised" */

        /* Relax every neighbour of u */
        for (int e = g->head[u]; e != -1; e = g->edges[e].next) {
            int v = g->edges[e].dest;
            int w = g->edges[e].weight;

            if (visited[v]) continue;

            /* Relaxation check — weaker (shorter) path found */
            if (dist[u] != INT_MAX && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;            /* update path tracking */
            }
        }
    }

    /* ── Build result ────────────────────────────────────────── */
    if (dist[dst] == INT_MAX) {
        result.found = false;   /* no path found */
        return result;
    }

    result.found        = true;
    result.total_weight = dist[dst];
    reconstruct_path(prev, dst, src, result.path, &result.length);
    return result;
}

/* ─── Output formatting ──────────────────────────────────────────── */

void print_result(PathResult *r, int src, int dst) {
    if (src == dst) {
        printf("%d\n", src);
        return;
    }
    if (!r->found) {
        printf("No path found\n");
        return;
    }
    for (int i = 0; i < r->length; i++) {
        if (i > 0) printf(" -> ");
        printf("%d", r->path[i]);
    }
    printf("\nTotal weight: %d\n", r->total_weight);
}
