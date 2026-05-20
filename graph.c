#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

/* ═══════════════════════════════════════════════════════════════════════════
   1.  DATA-STRUCTURE MANAGEMENT
   ═══════════════════════════════════════════════════════════════════════════ */

Node* createNode(int dest, int weight) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        fprintf(stderr, "Error: memory allocation failed in createNode\n");
        exit(EXIT_FAILURE);
    }
    newNode->dest   = dest;
    newNode->weight = weight;
    newNode->next   = NULL;
    return newNode;
}

Graph* initGraph(int vertices) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph) {
        fprintf(stderr, "Error: memory allocation failed in initGraph\n");
        exit(EXIT_FAILURE);
    }
    graph->vertices = vertices;
    graph->adjList  = (Node**)calloc(vertices, sizeof(Node*));
    if (!graph->adjList) {
        fprintf(stderr, "Error: memory allocation failed for adjacency list\n");
        free(graph);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < vertices; i++)
        graph->adjList[i] = NULL;
    return graph;
}

void addEdge(Graph* graph, int src, int dest, int weight) {
    if (weight < 0) {
        fprintf(stderr,
            "Error: negative weight (%d) on edge %d -> %d is not allowed\n",
            weight, src, dest);
        freeGraph(graph);
        exit(EXIT_FAILURE);
    }
    if (src < 0 || src >= graph->vertices ||
        dest < 0 || dest >= graph->vertices) {
        fprintf(stderr,
            "Error: vertex index out of range (src=%d, dest=%d, N=%d)\n",
            src, dest, graph->vertices);
        freeGraph(graph);
        exit(EXIT_FAILURE);
    }
    Node* newNode       = createNode(dest, weight);
    newNode->next       = graph->adjList[src];
    graph->adjList[src] = newNode;
}

/* ═══════════════════════════════════════════════════════════════════════════
   2.  FILE I/O & PARSING
   ═══════════════════════════════════════════════════════════════════════════ */

Graph* parseGraphFromFile(const char* filename, Query* query) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }
    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        fprintf(stderr, "Error: invalid header in file '%s'\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    Graph* graph = initGraph(N);
    for (int i = 0; i < M; i++) {
        int src, dest, weight;
        if (fscanf(fp, "%d %d %d", &src, &dest, &weight) != 3) {
            fprintf(stderr, "Error: malformed edge on line %d\n", i + 2);
            freeGraph(graph);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        addEdge(graph, src, dest, weight);
    }
    if (fscanf(fp, "%d %d", &query->src, &query->dst) != 2) {
        fprintf(stderr, "Error: missing query line in file '%s'\n", filename);
        freeGraph(graph);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    return graph;
}

/* ═══════════════════════════════════════════════════════════════════════════
   3.  ALGORITHM
   ═══════════════════════════════════════════════════════════════════════════ */

int findMinDistance(int dist[], int visited[], int n) {
    int minVal = INF;
    int minIdx = -1;
    for (int v = 0; v < n; v++) {
        if (!visited[v] && dist[v] <= minVal) {
            minVal = dist[v];
            minIdx = v;
        }
    }
    return minIdx;
}

/*
 * dijkstra – fills dist[] and prev[] arrays (does NOT print).
 * Matches the declaration in graph.h and the usage in gui.c.
 */
void dijkstra(Graph* graph, int startNode, int* dist, int* prev) {
    int n = graph->vertices;
    int* visited = (int*)calloc(n, sizeof(int));
    if (!visited) {
        fprintf(stderr, "Error: memory allocation failed in dijkstra\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        prev[i] = -1;
    }
    dist[startNode] = 0;

    for (int iter = 0; iter < n - 1; iter++) {
        int u = findMinDistance(dist, visited, n);
        if (u == -1) break;
        visited[u] = 1;
        Node* curr = graph->adjList[u];
        while (curr) {
            int v = curr->dest;
            int w = curr->weight;
            if (!visited[v] && dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
            }
            curr = curr->next;
        }
    }
    free(visited);
}

/* ═══════════════════════════════════════════════════════════════════════════
   4.  OUTPUT
   ═══════════════════════════════════════════════════════════════════════════ */

void printPath(int parent[], int j) {
    if (parent[j] == -1) { printf("%d", j); return; }
    printPath(parent, parent[j]);
    printf(" -> %d", j);
}

void displayResults(int dist[], int parent[], int start, int end, int n) {
    (void)n;
    if (start == end) { printf("%d\n0\n", start); return; }
    if (dist[end] == INF) { printf("No path found\n"); return; }
    printPath(parent, end);
    printf("\n%d\n", dist[end]);
}

/* ═══════════════════════════════════════════════════════════════════════════
   5.  CLEANUP
   ═══════════════════════════════════════════════════════════════════════════ */

void freeGraph(Graph* graph) {
    if (!graph) return;
    for (int i = 0; i < graph->vertices; i++) {
        Node* curr = graph->adjList[i];
        while (curr) { Node* tmp = curr; curr = curr->next; free(tmp); }
    }
    free(graph->adjList);
    free(graph);
}

/* ═══════════════════════════════════════════════════════════════════════════
   6.  MILESTONE 4 – Worker Parsing & Graph Ingestion
   ═══════════════════════════════════════════════════════════════════════════ */

Graph* InitializeGraph(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "InitializeGraph: cannot open '%s'\n", filename);
        return NULL;
    }
    int N = 0;
    if (fscanf(fp, "%d", &N) != 1 || N <= 0 || N > MAX_NODES) {
        fprintf(stderr, "InitializeGraph: invalid node count\n");
        fclose(fp);
        return NULL;
    }
    Graph* g = initGraph(N);
    if (!g) { fclose(fp); return NULL; }
    for (int u = 0; u < N; u++) {
        for (int v = 0; v < N; v++) {
            int w = 0;
            if (fscanf(fp, "%d", &w) != 1) {
                fprintf(stderr, "InitializeGraph: matrix read error at (%d,%d)\n", u, v);
                freeGraph(g);
                fclose(fp);
                return NULL;
            }
            if (w > 0)
                addEdge(g, u, v, w);
        }
    }
    fclose(fp);
    return g;
}

bool ParseInputRequest(const char* buffer, int numNodes, Query* outQuery) {
    if (!buffer || !outQuery) return false;
    int  src = 0, dst = 0;
    char extra = '\0';
    int parsed = sscanf(buffer, "%d %d %c", &src, &dst, &extra);
    if (parsed != 2) {
        fprintf(stderr, "ParseInputRequest: expected 'SRC DST', got: '%s'\n", buffer);
        return false;
    }
    if (src < 0 || src >= numNodes) {
        fprintf(stderr, "ParseInputRequest: src %d out of range [0,%d]\n",
                src, numNodes - 1);
        return false;
    }
    if (dst < 0 || dst >= numNodes) {
        fprintf(stderr, "ParseInputRequest: dst %d out of range [0,%d]\n",
                dst, numNodes - 1);
        return false;
    }
    outQuery->src = src;
    outQuery->dst = dst;
    return true;
}
