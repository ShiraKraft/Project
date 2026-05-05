//
// Created by student on 5/5/26.
//

#ifndef PROJECT_GRAPH_H
#define PROJECT_GRAPH_H

#include <stdbool.h>

/* ─── Constants ─────────────────────────────────────────────────── */

#define MAX_NODES 100
#define MAX_EDGES 500

/* ─── Data Structures ───────────────────────────────────────────── */

typedef struct {
    int dest;
    int weight;
    int next;   // index of next edge in adjacency list (-1 = none)
} Edge;

typedef struct {
    int num_nodes;
    int head[MAX_NODES];    // head[u] = index of first edge from u
    Edge edges[MAX_EDGES];
    int edge_count;
} Graph;

typedef struct {
    int path[MAX_NODES];
    int length;             // number of nodes in path
    int total_weight;
    bool found;
} PathResult;

/* ─── Graph Function Declarations ───────────────────────────────── */

void init_graph(Graph *g, int num_nodes);
void add_edge(Graph *g, int u, int v, int weight);

/* ─── Algorithm Function Declarations ──────────────────────────── */

int        find_min_distance(int dist[], bool visited[], int num_nodes);
PathResult run_dijkstra(Graph *g, int src, int dst);
void       reconstruct_path(int prev[], int dst, int src,
                            int *out_path, int *out_len);

/* ─── Output Function Declaration ───────────────────────────────── */

void print_result(PathResult *r, int src, int dst);

#endif //PROJECT_GRAPH_H