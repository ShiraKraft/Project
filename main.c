#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    Query  query;
    Graph* graph = parseGraphFromFile(argv[1], &query);

    int dist[MAX_NODES], prev[MAX_NODES];
    dijkstra(graph, query.src, dist, prev);
    displayResults(dist, prev, query.src, query.dst, graph->vertices);

    freeGraph(graph);
    return EXIT_SUCCESS;
}