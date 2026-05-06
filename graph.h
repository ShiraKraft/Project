#ifndef GRAPH_H
#define GRAPH_H

#define INF 99999999

// ── Node in adjacency list ──────────────────────────────────────────────────
typedef struct Node {
    int dest;
    int weight;
    struct Node* next;
} Node;

// ── Graph ───────────────────────────────────────────────────────────────────
typedef struct {
    int   vertices;
    Node** adjList;   // array of linked-list heads, size = vertices
} Graph;

// ── Query (src + dst read from file) ────────────────────────────────────────
typedef struct {
    int src;
    int dst;
} Query;

// ── Function declarations ────────────────────────────────────────────────────
Node*  createNode(int dest, int weight);
Graph* initGraph(int vertices);
void   addEdge(Graph* graph, int src, int dest, int weight);

Graph* parseGraphFromFile(const char* filename, Query* query);

int    findMinDistance(int dist[], int visited[], int n);
void   dijkstra(Graph* graph, int startNode, int endNode);

void   printPath(int parent[], int j);
void   displayResults(int dist[], int parent[], int start, int end, int n);

void   freeGraph(Graph* graph);

#endif // GRAPH_H
