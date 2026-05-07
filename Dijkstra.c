//
// Created by student on 5/5/26.
// Dijkstra.c
//
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "graph.h"

/* ─── Graph helpers ─────────────────────────────────────────────── */

void init_graph(CompactGraph *g, int num_nodes) {
    g->num_nodes  = num_nodes;
    g->edge_count = 0;
    for (int i = 0; i < num_nodes; i++)
        g->head[i] = -1;
}

void add_edge(CompactGraph *g, int u, int v, int weight) {
    g->edges[g->edge_count] = (Edge){ v, weight, g->head[u] };
    g->head[u] = g->edge_count++;

    g->edges[g->edge_count] = (Edge){ u, weight, g->head[v] };
    g->head[v] = g->edge_count++;
}

/* ─── 1. find_min_distance ──────────────────────────────────────── */
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

    for (int i = 0; i < len; i++)
        out_path[i] = tmp[len - 1 - i];

    *out_len = len;
}

/* ─── 2. run_dijkstra ───────────────────────────────────────────── */
PathResult run_dijkstra(CompactGraph *g, int src, int dst) {
    int  dist[MAX_NODES];
    int  prev[MAX_NODES];
    bool visited[MAX_NODES];
    PathResult result = { .found = false, .length = 0, .total_weight = 0 };

    for (int i = 0; i < g->num_nodes; i++) {
        dist[i]    = INT_MAX;
        prev[i]    = -1;
        visited[i] = false;
    }
    dist[src] = 0;

    if (src == dst) {
        result.found        = true;
        result.path[0]      = src;
        result.length       = 1;
        result.total_weight = 0;
        return result;
    }

    for (int iter = 0; iter < g->num_nodes; iter++) {
        int u = find_min_distance(dist, visited, g->num_nodes);
        if (u == -1 || dist[u] == INT_MAX) break;
        visited[u] = true;

        for (int e = g->head[u]; e != -1; e = g->edges[e].next) {
            int v = g->edges[e].dest;
            int w = g->edges[e].weight;
            if (visited[v]) continue;
            if (dist[u] != INT_MAX && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
            }
        }
    }

    if (dist[dst] == INT_MAX) {
        result.found = false;
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
    printf("\n%d\n", r->total_weight);
}