//
// graph.h  –  Unified header for OS Project (Milestone 1 + 2)
//
#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Constants
 * ═══════════════════════════════════════════════════════════════════ */
#define MAX_NODES 100
#define MAX_EDGES 500
#define INF       99999999

/* ═══════════════════════════════════════════════════════════════════
 *  Adjacency-list graph  (used by graph.c, gui.c, main.c)
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct Node {
    int dest;
    int weight;
    struct Node* next;
} Node;

typedef struct {
    int    vertices;
    Node** adjList;
} Graph;

typedef struct {
    int src;
    int dst;
} Query;

/* ═══════════════════════════════════════════════════════════════════
 *  Compact edge-array graph  (used by Dijkstra.c internally)
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    int dest;
    int weight;
    int next;
} Edge;

typedef struct {
    int  num_nodes;
    int  head[MAX_NODES];
    Edge edges[MAX_EDGES];
    int  edge_count;
} CompactGraph;

typedef struct {
    int  path[MAX_NODES];
    int  length;
    int  total_weight;
    bool found;
} PathResult;

/* ═══════════════════════════════════════════════════════════════════
 *  graph.c – declarations
 * ═══════════════════════════════════════════════════════════════════ */
Node*  createNode(int dest, int weight);
Graph* initGraph(int vertices);
void   addEdge(Graph* graph, int src, int dest, int weight);
Graph* parseGraphFromFile(const char* filename, Query* query);
int    findMinDistance(int dist[], int visited[], int n);

/*
 * Two dijkstra signatures:
 *   - main.c path: dijkstra(graph, src, dst)  prints result directly
 *   - gui.c  path: dijkstra(graph, src, dist, prev)  fills arrays
 *
 * We expose only the array-filling version; displayResults() handles output.
 */
void dijkstra(Graph* graph, int startNode, int* dist, int* prev);

void printPath(int parent[], int j);
void displayResults(int dist[], int parent[], int start, int end, int n);
void freeGraph(Graph* graph);

/* ═══════════════════════════════════════════════════════════════════
 *  Dijkstra.c – declarations
 * ═══════════════════════════════════════════════════════════════════ */
void       init_graph(CompactGraph *g, int num_nodes);
void       add_edge(CompactGraph *g, int u, int v, int weight);
int        find_min_distance(int dist[], bool visited[], int num_nodes);
PathResult run_dijkstra(CompactGraph *g, int src, int dst);
void       reconstruct_path(int prev[], int dst, int src,
                            int *out_path, int *out_len);
void       print_result(PathResult *r, int src, int dst);

#endif /* GRAPH_H */