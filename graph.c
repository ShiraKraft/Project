#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

/* ── createNode ─────────────────────────────────────────────────────────── */
Node* createNode(int dest, int weight) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "Error: malloc failed\n"); exit(EXIT_FAILURE); }
    n->dest   = dest;
    n->weight = weight;
    n->next   = NULL;
    return n;
}

/* ── initGraph ──────────────────────────────────────────────────────────── */
Graph* initGraph(int vertices) {
    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) { fprintf(stderr, "Error: malloc failed\n"); exit(EXIT_FAILURE); }
    g->vertices = vertices;
    g->adjList  = (Node**)calloc(vertices, sizeof(Node*));
    if (!g->adjList) { fprintf(stderr, "Error: calloc failed\n"); free(g); exit(EXIT_FAILURE); }
    return g;
}

/* ── addEdge ────────────────────────────────────────────────────────────── */
void addEdge(Graph* graph, int src, int dest, int weight) {
    if (weight < 0) {
        fprintf(stderr, "Error: negative weight (%d) on edge %d -> %d\n",
                weight, src, dest);
        freeGraph(graph);
        exit(EXIT_FAILURE);
    }
    if (src < 0 || src >= graph->vertices ||
        dest < 0 || dest >= graph->vertices) {
        fprintf(stderr, "Error: vertex index out of range\n");
        freeGraph(graph);
        exit(EXIT_FAILURE);
    }
    Node* n         = createNode(dest, weight);
    n->next         = graph->adjList[src];
    graph->adjList[src] = n;
}

/* ── parseGraphFromFile ─────────────────────────────────────────────────── */
Graph* parseGraphFromFile(const char* filename, Query* query) {
    FILE* fp = fopen(filename, "r");
    if (!fp) { fprintf(stderr, "Error: cannot open '%s'\n", filename); exit(EXIT_FAILURE); }

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        fprintf(stderr, "Error: invalid header\n"); fclose(fp); exit(EXIT_FAILURE);
    }

    Graph* graph = initGraph(N);
    for (int i = 0; i < M; i++) {
        int s, d, w;
        if (fscanf(fp, "%d %d %d", &s, &d, &w) != 3) {
            fprintf(stderr, "Error: malformed edge %d\n", i+2);
            freeGraph(graph); fclose(fp); exit(EXIT_FAILURE);
        }
        addEdge(graph, s, d, w);
    }

    if (fscanf(fp, "%d %d", &query->src, &query->dst) != 2) {
        fprintf(stderr, "Error: missing query line\n");
        freeGraph(graph); fclose(fp); exit(EXIT_FAILURE);
    }
    fclose(fp);
    return graph;
}

/* ── findMinDistance ────────────────────────────────────────────────────── */
int findMinDistance(int dist[], int visited[], int n) {
    int minVal = INF, minIdx = -1;
    for (int v = 0; v < n; v++) {
        if (!visited[v] && dist[v] <= minVal) {
            minVal = dist[v];
            minIdx = v;
        }
    }
    return minIdx;
}

/* ── dijkstra  (fills dist[] and prev[], does NOT print) ────────────────── */
void dijkstra(Graph* graph, int startNode, int* dist, int* prev) {
    int n = graph->vertices;
    int* visited = (int*)calloc(n, sizeof(int));
    if (!visited) { fprintf(stderr, "Error: malloc failed\n"); exit(EXIT_FAILURE); }

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        prev[i] = -1;
    }
    dist[startNode] = 0;

    for (int iter = 0; iter < n - 1; iter++) {
        int u = findMinDistance(dist, visited, n);
        if (u == -1) break;
        visited[u] = 1;

        Node* cur = graph->adjList[u];
        while (cur) {
            int v = cur->dest, w = cur->weight;
            if (!visited[v] && dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
            }
            cur = cur->next;
        }
    }
    free(visited);
}

/* ── printPath ──────────────────────────────────────────────────────────── */
void printPath(int parent[], int j) {
    if (parent[j] == -1) { printf("%d", j); return; }
    printPath(parent, parent[j]);
    printf(" -> %d", j);
}

/* ── displayResults ─────────────────────────────────────────────────────── */
void displayResults(int dist[], int parent[], int start, int end, int n) {
    (void)n;
    if (start == end) { printf("%d\n0\n", start); return; }
    if (dist[end] == INF) { printf("No path found\n"); return; }
    printPath(parent, end);
    printf("\n%d\n", dist[end]);
}

/* ── freeGraph ──────────────────────────────────────────────────────────── */
void freeGraph(Graph* graph) {
    if (!graph) return;
    for (int i = 0; i < graph->vertices; i++) {
        Node* cur = graph->adjList[i];
        while (cur) { Node* tmp = cur; cur = cur->next; free(tmp); }
    }
    free(graph->adjList);
    free(graph);
}