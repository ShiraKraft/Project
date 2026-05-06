#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

/* ═══════════════════════════════════════════════════════════════════════════
   1.  DATA-STRUCTURE MANAGEMENT
   ═══════════════════════════════════════════════════════════════════════════ */

/*
 * createNode – allocates and initialises a single adjacency-list node.
 *
 * Parameters:
 *   dest   – index of the destination vertex
 *   weight – weight of the edge
 *
 * Returns: pointer to the new Node, or exits on allocation failure.
 */
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

/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * initGraph – allocates the Graph struct and zeroes all adjacency-list heads.
 *
 * Parameters:
 *   vertices – number of vertices N in the graph
 *
 * Returns: pointer to the new Graph.
 */
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
    /* calloc already zero-initialises, but be explicit */
    for (int i = 0; i < vertices; i++)
        graph->adjList[i] = NULL;

    return graph;
}

/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * addEdge – inserts a directed edge src → dest with the given weight.
 *
 * Validation:
 *   Negative weights are invalid; the function prints an error and exits.
 *
 * The new node is prepended to src's adjacency list (O(1)).
 */
void addEdge(Graph* graph, int src, int dest, int weight) {
    /* ── Validation ── */
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

    Node* newNode        = createNode(dest, weight);
    newNode->next        = graph->adjList[src];
    graph->adjList[src]  = newNode;
}


/* ═══════════════════════════════════════════════════════════════════════════
   2.  FILE I/O & PARSING
   ═══════════════════════════════════════════════════════════════════════════ */

/*
 * parseGraphFromFile – reads a graph and a Dijkstra query from a text file.
 *
 * File format:
 *   Line 1       :  N  M          (vertices, edges)
 *   Lines 2..M+1 :  src dst weight
 *   Last line    :  src dst        (query pair)
 *
 * Parameters:
 *   filename – path to the input file
 *   query    – output parameter; filled with the src/dst query
 *
 * Returns: pointer to the loaded Graph.
 */
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
        addEdge(graph, src, dest, weight);   /* validation happens inside */
    }

    /* Read the query line */
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

/*
 * findMinDistance – returns the index of the unvisited vertex with the
 *                   smallest tentative distance.
 *
 * Parameters:
 *   dist    – array of current distances, size n
 *   visited – array of visited flags,    size n
 *   n       – number of vertices
 *
 * Returns: index of the minimum-distance unvisited vertex, or -1 if none.
 */
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

/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * dijkstra – finds the shortest path from startNode to endNode and prints
 *            the result via displayResults().
 *
 * Uses a simple O(V²) implementation (appropriate for V ≤ 15 per spec).
 * Internally builds:
 *   dist[]   – shortest distance from startNode to every vertex
 *   parent[] – predecessor array for path reconstruction
 */
void dijkstra(Graph* graph, int startNode, int endNode) {
    int n = graph->vertices;

    int* dist    = (int*)malloc(n * sizeof(int));
    int* visited = (int*)malloc(n * sizeof(int));
    int* parent  = (int*)malloc(n * sizeof(int));

    if (!dist || !visited || !parent) {
        fprintf(stderr, "Error: memory allocation failed in dijkstra\n");
        free(dist); free(visited); free(parent);
        exit(EXIT_FAILURE);
    }

    /* Initialise */
    for (int i = 0; i < n; i++) {
        dist[i]    = INF;
        visited[i] = 0;
        parent[i]  = -1;
    }
    dist[startNode] = 0;

    /* Main loop: relax N times */
    for (int iter = 0; iter < n - 1; iter++) {
        int u = findMinDistance(dist, visited, n);
        if (u == -1) break;          /* remaining vertices are unreachable */
        visited[u] = 1;

        /* Relax all neighbours of u */
        Node* curr = graph->adjList[u];
        while (curr) {
            int v = curr->dest;
            int w = curr->weight;
            if (!visited[v] && dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v]   = dist[u] + w;
                parent[v] = u;
            }
            curr = curr->next;
        }
    }

    displayResults(dist, parent, startNode, endNode, n);

    free(dist);
    free(visited);
    free(parent);
}


/* ═══════════════════════════════════════════════════════════════════════════
   4.  OUTPUT
   ═══════════════════════════════════════════════════════════════════════════ */

/*
 * printPath – recursively prints the path from the source to vertex j
 *             using the parent[] array built by Dijkstra.
 *
 * Example output fragment:  0 -> 2 -> 5
 * (No trailing newline; caller handles that.)
 */
void printPath(int parent[], int j) {
    if (parent[j] == -1) {
        /* Base case: j is the source node */
        printf("%d", j);
        return;
    }
    printPath(parent, parent[j]);
    printf(" -> %d", j);
}

/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * displayResults – central output function.
 *
 * Prints:
 *   • The path and total weight   – if a path exists
 *   • "No path found"             – if the destination is unreachable
 *   • Just the node + weight 0    – if start == end
 *
 * Parameters:
 *   dist   – distances array from dijkstra()
 *   parent – parent array from dijkstra()
 *   start  – source vertex index
 *   end    – destination vertex index
 *   n      – total number of vertices (used for bounds; unused here but
 *             kept for a consistent signature)
 */
void displayResults(int dist[], int parent[], int start, int end, int n) {
    (void)n;   /* suppress unused-parameter warning */

    /* Case: source == destination */
    if (start == end) {
        printf("%d\n0\n", start);
        return;
    }

    /* Case: unreachable */
    if (dist[end] == INF) {
        printf("No path found\n");
        return;
    }

    /* Case: valid path found */
    printPath(parent, end);
    printf("\n%d\n", dist[end]);
}


/* ═══════════════════════════════════════════════════════════════════════════
   5.  CLEANUP
   ═══════════════════════════════════════════════════════════════════════════ */

/*
 * freeGraph – releases all dynamically allocated memory owned by the graph.
 *
 * Walks every adjacency list and frees each Node, then frees the adjList
 * array and the Graph struct itself.
 */
void freeGraph(Graph* graph) {
    if (!graph) return;

    for (int i = 0; i < graph->vertices; i++) {
        Node* curr = graph->adjList[i];
        while (curr) {
            Node* tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    free(graph->adjList);
    free(graph);
}
