/*
 * main.c – Milestones 1, 2, and 3
 *
 * Usage:
 *   ./dijkstra <file>          (milestone 1 – terminal only)
 *   ./sim      <file>          (milestone 2 – static GUI)
 *   ./sim -schd fcfs <file>    (milestone 7 placeholder)
 *   ./sim -schd sjf  <file>    (milestone 7 placeholder)
 *
 * Milestone 3 is activated automatically when the GUI window is open:
 * click "START BREACH" to run the cyber-animation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "gui.h"

static void usage(const char* prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s <file>                (milestone 1 – terminal Dijkstra)\n"
        "  sim <file>               (milestones 2-6 – GUI)\n"
        "  sim -schd fcfs <file>    (milestone 7)\n"
        "  sim -schd sjf  <file>    (milestone 7)\n",
        prog);
}

static int run_milestone1(const char* filename)
{
    Query  query;
    Graph* graph = parseGraphFromFile(filename, &query);
    if (!graph) return EXIT_FAILURE;

    int dist[MAX_NODES], prev[MAX_NODES];
    dijkstra(graph, query.src, dist, prev);
    displayResults(dist, prev, query.src, query.dst, graph->vertices);

    freeGraph(graph);
    return EXIT_SUCCESS;
}

static int run_gui(const char* filename)
{
    Query  query;
    Graph* graph = parseGraphFromFile(filename, &query);
    if (!graph) return EXIT_FAILURE;

    if (!ValidateInput(graph)) { freeGraph(graph); return EXIT_FAILURE; }

    int n = graph->vertices;
    NodeVisual* nodes = (NodeVisual*)calloc(n, sizeof(NodeVisual));
    if (!nodes) { freeGraph(graph); return EXIT_FAILURE; }

    InitGraphWindow(1100, 700, "Network Intrusion Simulator – Milestone 3");

    int   screenW = GetScreenWidth();
    int   screenH = GetScreenHeight();
    float cx      = screenW  * 0.48f;
    float cy      = screenH  * 0.50f;
    float radius  = (screenW < screenH ? screenW : screenH) * 0.36f;

    CalculateCircularLayout(nodes, n, (Vector2){ cx, cy }, radius);

    for (int i = 0; i < n; i++) {
        nodes[i].id = i;
        snprintf(nodes[i].label, sizeof(nodes[i].label), "SRV%d", i);
    }

    PathResult pathResult;
    bool pathFound = GetDijkstraPathData(graph, nodes,
                                         query.src, query.dst, &pathResult);

    if (!pathFound)
        printf("No path found from %d to %d\n", query.src, query.dst);

    Rectangle btnRect = { 20.0f, (float)(GetScreenHeight() - 60), 200.0f, 44.0f };

    while (!WindowShouldClose())
    {
        if (IsWindowResized()) {
            screenW  = GetScreenWidth();
            screenH  = GetScreenHeight();
            cx       = screenW  * 0.48f;
            cy       = screenH  * 0.50f;
            radius   = (screenW < screenH ? screenW : screenH) * 0.36f;
            CalculateCircularLayout(nodes, n, (Vector2){ cx, cy }, radius);
            for (int i = 0; i < n; i++) {
                nodes[i].id = i;
                snprintf(nodes[i].label, sizeof(nodes[i].label), "SRV%d", i);
            }
            btnRect.y = (float)(screenH - 60);
            if (pathFound)
                exploitPos = (Vector2){ nodes[query.src].x, nodes[query.src].y };
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnRect) && pathFound) {
                if (animationDone) {
                    animationDone    = false;
                    currentPathIndex = 0;
                    moveTimer        = 0.0f;
                    nodeTimer        = 0.0f;
                    onNodePause      = false;
                    exploitPos       = (Vector2){ nodes[query.src].x,
                                                  nodes[query.src].y };
                    isAnimating = true;
                } else {
                    isAnimating = !isAnimating;
                }
            }
        }

        if (isAnimating && pathFound)
            UpdateAttackAnimation(graph, nodes, &pathResult);

        DrawCyberGUI(graph, nodes, &pathResult, btnRect);
    }

    freeGraph(graph);
    free(nodes);
    CloseWindow();
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    if (argc < 2) { usage(argv[0]); return EXIT_FAILURE; }

    const char* filename = NULL;

    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 4 && strcmp(argv[1], "-schd") == 0) {
        filename = argv[3];
        printf("[INFO] Scheduler '%s' selected (milestone 7 – not yet implemented).\n",
               argv[2]);
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];

    if (strcmp(prog, "dijkstra") == 0)
        return run_milestone1(filename);

    return run_gui(filename);
}