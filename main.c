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

/* ── Forward declaration for the milestone-1 dijkstra entry point ──────── */
/* (implemented in graph.c via dijkstra() / displayResults())              */

/* ═══════════════════════════════════════════════════════════════════════════
 *  helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestone 1 – terminal Dijkstra  (./dijkstra <file>)
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestones 2 & 3 – GUI  (./sim <file>)
 * ═══════════════════════════════════════════════════════════════════════════ */

static int run_gui(const char* filename)
{
    /* ── Load graph ──────────────────────────────────────────────────────── */
    Query  query;
    Graph* graph = parseGraphFromFile(filename, &query);
    if (!graph) return EXIT_FAILURE;

    if (!ValidateInput(graph)) { freeGraph(graph); return EXIT_FAILURE; }

    /* ── Layout ──────────────────────────────────────────────────────────── */
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

    /* Label nodes */
    for (int i = 0; i < n; i++) {
        nodes[i].id = i;
        snprintf(nodes[i].label, sizeof(nodes[i].label), "SRV%d", i);
    }

    /* ── Run Dijkstra once and build PathResult for the animator ─────────── */
    PathResult pathResult;
    bool pathFound = GetDijkstraPathData(graph, nodes,
                                         query.src, query.dst, &pathResult);
    /* pathResult.path[], .length, .total_weight, .found now populated      */

    if (!pathFound) {
        printf("No path found from %d to %d\n", query.src, query.dst);
        /* Still open the window so the graph is visible */
    }

    /* ── Button rectangle (fixed position; hit-tested in the loop) ───────── */
    Rectangle btnRect = { 20.0f, (float)(GetScreenHeight() - 60), 200.0f, 44.0f };

    /* ── Main game loop ─────────────────────────────────────────────────── */
    while (!WindowShouldClose())
    {
        /* Handle window resize */
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

            /* Re-seed exploit position at source after resize */
            if (pathFound)
                exploitPos = (Vector2){ nodes[query.src].x, nodes[query.src].y };
        }

        /* ── Input: Play/Stop button ───────────────────────────────────── */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnRect) && pathFound) {
                if (animationDone) {
                    /* Restart the animation */
                    animationDone    = false;
                    currentPathIndex = 0;
                    moveTimer        = 0.0f;
                    nodeTimer        = 0.0f;
                    onNodePause      = false;
                    exploitPos       = (Vector2){ nodes[query.src].x,
                                                  nodes[query.src].y };
                    isAnimating = true;
                } else {
                    isAnimating = !isAnimating;   /* toggle Play/Stop */
                }
            }
        }

        /* ── Update ────────────────────────────────────────────────────── */
        if (isAnimating && pathFound)
            UpdateAttackAnimation(graph, nodes, &pathResult);
            /* UpdateAttackAnimation accesses:
               PathResult.path[], .length  (via pathResult)
               Graph.adjList              (edge weight lookup)
               NodeVisual[].x/.y          (screen interpolation) */

        /* ── Draw ──────────────────────────────────────────────────────── */
        DrawCyberGUI(graph, nodes, &pathResult, btnRect);
        /* DrawCyberGUI accesses:
           Graph.adjList, .vertices
           NodeVisual[].x/.y/.radius/.color/.label
           PathResult.path[], .length, .total_weight
           exploitPos, isAnimating, animationDone, currentPathIndex */
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    freeGraph(graph);
    free(nodes);
    CloseWindow();
    return EXIT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[])
{
    if (argc < 2) { usage(argv[0]); return EXIT_FAILURE; }

    /* Milestone 7 scheduler flag (placeholder) */
    const char* filename = NULL;

    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 4 && strcmp(argv[1], "-schd") == 0) {
        /* -schd fcfs/sjf <file> – scheduler to be implemented in milestone 7 */
        filename = argv[3];
        printf("[INFO] Scheduler '%s' selected (milestone 7 – not yet implemented).\n",
               argv[2]);
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Determine which binary was invoked */
    const char* prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];

    if (strcmp(prog, "dijkstra") == 0) {
        /* Milestone 1: terminal-only Dijkstra */
        return run_milestone1(filename);
    }

    /* Milestones 2 / 3 / 7 : GUI */
    return run_gui(filename);
}/*
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

/* ── Forward declaration for the milestone-1 dijkstra entry point ──────── */
/* (implemented in graph.c via dijkstra() / displayResults())              */

/* ═══════════════════════════════════════════════════════════════════════════
 *  helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestone 1 – terminal Dijkstra  (./dijkstra <file>)
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestones 2 & 3 – GUI  (./sim <file>)
 * ═══════════════════════════════════════════════════════════════════════════ */

static int run_gui(const char* filename)
{
    /* ── Load graph ──────────────────────────────────────────────────────── */
    Query  query;
    Graph* graph = parseGraphFromFile(filename, &query);
    if (!graph) return EXIT_FAILURE;

    if (!ValidateInput(graph)) { freeGraph(graph); return EXIT_FAILURE; }

    /* ── Layout ──────────────────────────────────────────────────────────── */
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

    /* Label nodes */
    for (int i = 0; i < n; i++) {
        nodes[i].id = i;
        snprintf(nodes[i].label, sizeof(nodes[i].label), "SRV%d", i);
    }

    /* ── Run Dijkstra once and build PathResult for the animator ─────────── */
    PathResult pathResult;
    bool pathFound = GetDijkstraPathData(graph, nodes,
                                         query.src, query.dst, &pathResult);
    /* pathResult.path[], .length, .total_weight, .found now populated      */

    if (!pathFound) {
        printf("No path found from %d to %d\n", query.src, query.dst);
        /* Still open the window so the graph is visible */
    }

    /* ── Button rectangle (fixed position; hit-tested in the loop) ───────── */
    Rectangle btnRect = { 20.0f, (float)(GetScreenHeight() - 60), 200.0f, 44.0f };

    /* ── Main game loop ─────────────────────────────────────────────────── */
    while (!WindowShouldClose())
    {
        /* Handle window resize */
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

            /* Re-seed exploit position at source after resize */
            if (pathFound)
                exploitPos = (Vector2){ nodes[query.src].x, nodes[query.src].y };
        }

        /* ── Input: Play/Stop button ───────────────────────────────────── */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnRect) && pathFound) {
                if (animationDone) {
                    /* Restart the animation */
                    animationDone    = false;
                    currentPathIndex = 0;
                    moveTimer        = 0.0f;
                    nodeTimer        = 0.0f;
                    onNodePause      = false;
                    exploitPos       = (Vector2){ nodes[query.src].x,
                                                  nodes[query.src].y };
                    isAnimating = true;
                } else {
                    isAnimating = !isAnimating;   /* toggle Play/Stop */
                }
            }
        }

        /* ── Update ────────────────────────────────────────────────────── */
        if (isAnimating && pathFound)
            UpdateAttackAnimation(graph, nodes, &pathResult);
            /* UpdateAttackAnimation accesses:
               PathResult.path[], .length  (via pathResult)
               Graph.adjList              (edge weight lookup)
               NodeVisual[].x/.y          (screen interpolation) */

        /* ── Draw ──────────────────────────────────────────────────────── */
        DrawCyberGUI(graph, nodes, &pathResult, btnRect);
        /* DrawCyberGUI accesses:
           Graph.adjList, .vertices
           NodeVisual[].x/.y/.radius/.color/.label
           PathResult.path[], .length, .total_weight
           exploitPos, isAnimating, animationDone, currentPathIndex */
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    freeGraph(graph);
    free(nodes);
    CloseWindow();
    return EXIT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[])
{
    if (argc < 2) { usage(argv[0]); return EXIT_FAILURE; }

    /* Milestone 7 scheduler flag (placeholder) */
    const char* filename = NULL;

    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 4 && strcmp(argv[1], "-schd") == 0) {
        /* -schd fcfs/sjf <file> – scheduler to be implemented in milestone 7 */
        filename = argv[3];
        printf("[INFO] Scheduler '%s' selected (milestone 7 – not yet implemented).\n",
               argv[2]);
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Determine which binary was invoked */
    const char* prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];

    if (strcmp(prog, "dijkstra") == 0) {
        /* Milestone 1: terminal-only Dijkstra */
        return run_milestone1(filename);
    }

    /* Milestones 2 / 3 / 7 : GUI */
    return run_gui(filename);
}