/*
 * gui.c  –  Milestone 2: GUI, System & QA Functions
 *
 * Covers:
 *   1. GUI & Visualization  (Raylib)
 *   2. System & Terminal Formatting
 *   3. Quality Assurance & Memory Management
 */

#include "gui.h"
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 1 – GUI & Visualization (Raylib)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * InitGraphWindow
 * ---------------
 * Opens a Raylib window, sets 60 FPS, and enables 4× anti-aliasing so node
 * circles look smooth.  Call this once before the game loop.
 */
void InitGraphWindow(int width, int height, const char* title)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);
    SetTargetFPS(60);
}

/*
 * CalculateCircularLayout
 * -----------------------
 * Distributes `numNodes` NodeVisual structs evenly around a circle defined
 * by `center` and `radius`.  Each node's (x, y) is set; the caller is
 * responsible for setting node IDs / labels before calling this function.
 *
 * Angle starts at -π/2 so the first node appears at the top of the circle.
 */
void CalculateCircularLayout(NodeVisual* nodes, int numNodes,
                             Vector2 center, float radius)
{
    if (!nodes || numNodes <= 0) return;

    for (int i = 0; i < numNodes; i++) {
        float angle = (float)i * (2.0f * PI / (float)numNodes) - (PI / 2.0f);
        nodes[i].x      = center.x + radius * cosf(angle);
        nodes[i].y      = center.y + radius * sinf(angle);
        nodes[i].radius = 24.0f;          /* default visual radius */
        nodes[i].color  = (Color){ 70, 130, 180, 255 }; /* steel-blue */
    }
}

/*
 * DrawGraphNode
 * -------------
 * Draws a filled circle for the node, a white outline for contrast, and the
 * node label centred inside.
 */
void DrawGraphNode(NodeVisual node)
{
    /* Shadow */
    DrawCircle((int)node.x + 3, (int)node.y + 3,
               node.radius, (Color){ 0, 0, 0, 60 });

    /* Fill */
    DrawCircleV((Vector2){ node.x, node.y }, node.radius, node.color);

    /* Outline */
    DrawCircleLinesV((Vector2){ node.x, node.y }, node.radius, WHITE);

    /* Label – centred */
    int fontSize = 16;
    int textW    = MeasureText(node.label, fontSize);
    DrawText(node.label,
             (int)(node.x - textW / 2),
             (int)(node.y - fontSize / 2),
             fontSize, WHITE);
}

/*
 * DrawDirectedEdge
 * ----------------
 * Draws a line from `start` to `end`, an arrowhead at the destination, and
 * the weight label at the midpoint of the edge.
 *
 * The arrowhead is constructed from two short lines rotated ±25° around the
 * direction vector so it always points toward `end`.
 */
void DrawDirectedEdge(Vector2 start, Vector2 end, int weight)
{
    /* ── Line ─────────────────────────────────────────────────────────── */
    Color edgeColor = (Color){ 200, 200, 200, 200 };
    DrawLineV(start, end, edgeColor);

    /* ── Arrowhead ────────────────────────────────────────────────────── */
    float arrowLen   = 14.0f;
    float arrowAngle = 25.0f * DEG2RAD;

    /* Direction from start → end */
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;             /* coincident points – skip */

    /* Unit vector pointing backward (toward start) */
    float ux = -dx / len;
    float uy = -dy / len;

    /* Two arrow wings */
    float cosA = cosf(arrowAngle), sinA = sinf(arrowAngle);

    Vector2 wing1 = {
        end.x + arrowLen * (ux * cosA - uy * sinA),
        end.y + arrowLen * (ux * sinA + uy * cosA)
    };
    Vector2 wing2 = {
        end.x + arrowLen * (ux * cosA + uy * sinA),
        end.y + arrowLen * (-ux * sinA + uy * cosA)
    };

    DrawLineV(end, wing1, edgeColor);
    DrawLineV(end, wing2, edgeColor);

    /* ── Weight label ─────────────────────────────────────────────────── */
    Vector2 mid    = { (start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f };
    char    wLabel[16];
    snprintf(wLabel, sizeof(wLabel), "%d", weight);

    int   fontSize = 14;
    int   textW    = MeasureText(wLabel, fontSize);
    Color bgColor  = (Color){ 30, 30, 30, 180 };

    /* Tiny background pill so the number is legible over edges */
    DrawRectangle((int)(mid.x - textW / 2 - 3),
                  (int)(mid.y - fontSize / 2 - 2),
                  textW + 6, fontSize + 4, bgColor);
    DrawText(wLabel,
             (int)(mid.x - textW / 2),
             (int)(mid.y - fontSize / 2),
             fontSize, YELLOW);
}

/*
 * RenderFrame
 * -----------
 * Called every iteration of the game loop.  Clears the screen, draws all
 * edges first (so nodes appear on top), then draws all nodes.
 */
void RenderFrame(const Graph* g, const NodeVisual* nodes)
{
    if (!g || !nodes) return;

    BeginDrawing();
    ClearBackground((Color){ 20, 20, 30, 255 }); /* dark navy background */

    /* ── Draw all edges ─────────────────────────────────────────────── */
    for (int u = 0; u < g->numVertices; u++) {
        AdjNode* cur = g->adjList[u];
        while (cur) {
            int v = cur->dest;

            /* Shrink endpoints so the arrow touches the node circle rim */
            Vector2 from = { nodes[u].x, nodes[u].y };
            Vector2 to   = { nodes[v].x, nodes[v].y };

            float dx  = to.x - from.x;
            float dy  = to.y - from.y;
            float len = sqrtf(dx * dx + dy * dy);

            if (len > 1e-4f) {
                float r = nodes[v].radius;
                to.x -= (dx / len) * r;
                to.y -= (dy / len) * r;
            }

            DrawDirectedEdge(from, to, cur->weight);
            cur = cur->next;
        }
    }

    /* ── Draw all nodes ─────────────────────────────────────────────── */
    for (int i = 0; i < g->numVertices; i++) {
        DrawGraphNode(nodes[i]);
    }

    /* ── HUD ────────────────────────────────────────────────────────── */
    DrawText("Graph Visualizer – Milestone 2",
             10, 10, 18, (Color){ 180, 180, 180, 255 });
    DrawFPS(10, 34);

    EndDrawing();
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 2 – System & Terminal Formatting
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * PrintFinalPath
 * --------------
 * Outputs the shortest path in the format:
 *     src -> n1 -> n2 -> dst
 *     Total weight: W
 *
 * `path` contains node IDs from source (index 0) to destination (index
 * pathSize-1).
 */
void PrintFinalPath(const int* path, int pathSize, int totalWeight)
{
    if (!path || pathSize <= 0) {
        printf("No path found\n");
        return;
    }

    for (int i = 0; i < pathSize; i++) {
        printf("%d", path[i]);
        if (i < pathSize - 1) printf(" -> ");
    }
    printf("\nTotal weight: %d\n", totalWeight);
}

/*
 * HandleSpecialCases
 * ------------------
 * Prints the appropriate message for two edge cases:
 *   • src == dst  →  distance is trivially 0
 *   • pathFound == 0  →  destination is unreachable
 */
void HandleSpecialCases(int src, int dst, int pathFound)
{
    if (src == dst) {
        printf("0\n");
        return;
    }
    if (!pathFound) {
        printf("No path found\n");
    }
}

/*
 * ValidateInput
 * -------------
 * Walks the entire adjacency list and returns false (+ error message) if any
 * edge weight is negative, since Dijkstra's algorithm requires non-negative
 * weights.
 */
bool ValidateInput(const Graph* g)
{
    if (!g) {
        fprintf(stderr, "Error: NULL graph pointer.\n");
        return false;
    }

    for (int u = 0; u < g->numVertices; u++) {
        AdjNode* cur = g->adjList[u];
        while (cur) {
            if (cur->weight < 0) {
                fprintf(stderr,
                    "Error: negative weight (%d) on edge %d→%d. "
                    "Dijkstra requires non-negative weights.\n",
                    cur->weight, u, cur->dest);
                return false;
            }
            cur = cur->next;
        }
    }
    return true;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 3 – Quality Assurance & Memory Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * FreeGraphMemory
 * ---------------
 * Releases every node in every adjacency list, then frees the list array
 * itself.  Safe to call even if some lists are NULL.
 */
void FreeGraphMemory(Graph* g)
{
    if (!g) return;

    for (int i = 0; i < g->numVertices; i++) {
        AdjNode* cur = g->adjList[i];
        while (cur) {
            AdjNode* next = cur->next;
            free(cur);
            cur = next;
        }
        g->adjList[i] = NULL;
    }

    free(g->adjList);
    g->adjList      = NULL;
    g->numVertices  = 0;
}

/*
 * CleanupResources
 * ----------------
 * Frees the NodeVisual array and the path array produced by the shortest-path
 * algorithm.  Also closes the Raylib window if one is open.
 */
void CleanupResources(NodeVisual* nodes, int* path)
{
    free(nodes);
    free(path);

    if (IsWindowReady()) {
        CloseWindow();
    }
}

/* ── Internal test helpers ────────────────────────────────────────────────── */

/*
 * build_test_graph  –  helper used only by RunSystemTests.
 * Creates a simple graph from an adjacency-list specification so the tests
 * are self-contained and do not depend on external files.
 *
 *  edges[][0] = src, edges[][1] = dst, edges[][2] = weight
 */
static Graph* build_test_graph(int numV,
                               int edges[][3], int numEdges)
{
    Graph* g = (Graph*)malloc(sizeof(Graph));
    assert(g);
    g->numVertices = numV;
    g->adjList     = (AdjNode**)calloc(numV, sizeof(AdjNode*));
    assert(g->adjList);

    for (int e = 0; e < numEdges; e++) {
        int u = edges[e][0], v = edges[e][1], w = edges[e][2];
        AdjNode* node = (AdjNode*)malloc(sizeof(AdjNode));
        assert(node);
        node->dest   = v;
        node->weight = w;
        node->next   = g->adjList[u];
        g->adjList[u] = node;
    }
    return g;
}

static void run_test(const char* name,
                     int numV, int edges[][3], int numEdges,
                     int src, int dst,
                     int expectReachable, int expectWeight)
{
    printf("\n[TEST] %s\n", name);

    Graph* g = build_test_graph(numV, edges, numEdges);

    /* Validate (no negative weights expected in these tests) */
    bool valid = ValidateInput(g);
    assert(valid && "test graph must be valid");

    /* Run Dijkstra */
    int  dist[128], prev[128];
    Dijkstra(g, src, dist, prev);   /* defined in graph.c */

    /* Reconstruct path */
    int path[128], pathSize = 0;
    if (dist[dst] == INF) {
        HandleSpecialCases(src, dst, 0);
        assert(!expectReachable && "expected unreachable");
    } else if (src == dst) {
        HandleSpecialCases(src, dst, 1);
        assert(expectWeight == 0);
    } else {
        /* Back-trace */
        int cur = dst;
        while (cur != -1) {
            path[pathSize++] = cur;
            cur = prev[cur];
        }
        /* Reverse */
        for (int l = 0, r = pathSize - 1; l < r; l++, r--) {
            int tmp = path[l]; path[l] = path[r]; path[r] = tmp;
        }
        PrintFinalPath(path, pathSize, dist[dst]);
        assert(expectReachable  && "expected reachable path");
        assert(dist[dst] == expectWeight && "weight mismatch");
    }

    FreeGraphMemory(g);
    free(g);
    printf("[PASS] %s\n", name);
}

/*
 * RunSystemTests
 * --------------
 * Runs 4 self-contained tests that cover:
 *   T1 – normal shortest path
 *   T2 – src == dst (trivial case)
 *   T3 – disconnected graph (no path)
 *   T4 – negative-weight detection
 */
void RunSystemTests(void)
{
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║       Running System Tests           ║\n");
    printf("╚══════════════════════════════════════╝\n");

    /* ── T1: Normal path: 0→1→3  weight = 1+2 = 3 ─────────────────── */
    {
        int edges[][3] = { {0,1,1}, {0,2,4}, {1,3,2}, {2,3,1} };
        run_test("T1 – Normal shortest path",
                 4, edges, 4,
                 /*src=*/0, /*dst=*/3,
                 /*expectReachable=*/1, /*expectWeight=*/3);
    }

    /* ── T2: src == dst ─────────────────────────────────────────────── */
    {
        int edges[][3] = { {0,1,5} };
        run_test("T2 – Source equals destination",
                 2, edges, 1,
                 /*src=*/0, /*dst=*/0,
                 /*expectReachable=*/1, /*expectWeight=*/0);
    }

    /* ── T3: No path (disconnected graph) ───────────────────────────── */
    {
        /* Nodes 0 and 1 exist but there is no edge between them */
        int edges[][3] = { {0,0,0} }; /* dummy self-loop to satisfy helper */
        Graph* g = build_test_graph(2, edges, 0); /* 0 edges */
        printf("\n[TEST] T3 – No path exists\n");
        int dist[2], prev[2];
        Dijkstra(g, 0, dist, prev);
        HandleSpecialCases(0, 1, dist[1] == INF ? 0 : 1);
        assert(dist[1] == INF && "node 1 must be unreachable");
        FreeGraphMemory(g);
        free(g);
        printf("[PASS] T3 – No path exists\n");
    }

    /* ── T4: Negative weight detection ──────────────────────────────── */
    {
        printf("\n[TEST] T4 – Negative weight detection\n");
        int edges[][3] = { {0,1,-3}, {1,2,2} };
        Graph* g = build_test_graph(3, edges, 2);
        bool valid = ValidateInput(g);
        assert(!valid && "negative weight must be rejected");
        FreeGraphMemory(g);
        free(g);
        printf("[PASS] T4 – Negative weight correctly rejected\n");
    }

    printf("\n╔══════════════════════════════════════╗\n");
    printf("║     All 4 system tests passed ✓      ║\n");
    printf("╚══════════════════════════════════════╝\n\n");
}
