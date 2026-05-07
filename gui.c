/*
 * gui.c  –  Milestone 2: GUI, System & QA Functions
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

void InitGraphWindow(int width, int height, const char* title)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);
    SetTargetFPS(60);
}

void CalculateCircularLayout(NodeVisual* nodes, int numNodes,
                             Vector2 center, float radius)
{
    if (!nodes || numNodes <= 0) return;

    for (int i = 0; i < numNodes; i++) {
        float angle = (float)i * (2.0f * PI / (float)numNodes) - (PI / 2.0f);
        nodes[i].x      = center.x + radius * cosf(angle);
        nodes[i].y      = center.y + radius * sinf(angle);
        nodes[i].radius = 24.0f;
        nodes[i].color  = (Color){ 70, 130, 180, 255 };
    }
}

void DrawGraphNode(NodeVisual node)
{
    DrawCircle((int)node.x + 3, (int)node.y + 3,
               node.radius, (Color){ 0, 0, 0, 60 });
    DrawCircleV((Vector2){ node.x, node.y }, node.radius, node.color);
    DrawCircleLinesV((Vector2){ node.x, node.y }, node.radius, WHITE);

    int fontSize = 16;
    int textW    = MeasureText(node.label, fontSize);
    DrawText(node.label,
             (int)(node.x - textW / 2),
             (int)(node.y - fontSize / 2),
             fontSize, WHITE);
}

void DrawDirectedEdge(Vector2 start, Vector2 end, int weight)
{
    Color edgeColor = (Color){ 200, 200, 200, 200 };
    DrawLineV(start, end, edgeColor);

    float arrowLen   = 14.0f;
    float arrowAngle = 25.0f * DEG2RAD;

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;

    float ux = -dx / len;
    float uy = -dy / len;

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

    Vector2 mid    = { (start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f };
    char    wLabel[16];
    snprintf(wLabel, sizeof(wLabel), "%d", weight);

    int   fontSize = 14;
    int   textW    = MeasureText(wLabel, fontSize);
    Color bgColor  = (Color){ 30, 30, 30, 180 };

    DrawRectangle((int)(mid.x - textW / 2 - 3),
                  (int)(mid.y - fontSize / 2 - 2),
                  textW + 6, fontSize + 4, bgColor);
    DrawText(wLabel,
             (int)(mid.x - textW / 2),
             (int)(mid.y - fontSize / 2),
             fontSize, YELLOW);
}

void RenderFrame(const Graph* g, const NodeVisual* nodes)
{
    if (!g || !nodes) return;

    BeginDrawing();
    ClearBackground((Color){ 20, 20, 30, 255 });

    /* Draw all edges */
    for (int u = 0; u < g->vertices; u++) {          /* vertices (not numVertices) */
        Node* cur = g->adjList[u];                    /* Node* (not AdjNode*) */
        while (cur) {
            int v = cur->dest;

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

    /* Draw all nodes */
    for (int i = 0; i < g->vertices; i++) {
        DrawGraphNode(nodes[i]);
    }

    DrawText("Graph Visualizer – Milestone 2",
             10, 10, 18, (Color){ 180, 180, 180, 255 });
    DrawFPS(10, 34);

    EndDrawing();
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 2 – System & Terminal Formatting
 * ═══════════════════════════════════════════════════════════════════════════ */

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
    printf("\n%d\n", totalWeight);
}

void HandleSpecialCases(int src, int dst, int pathFound)
{
    if (src == dst) {
        printf("%d\n0\n", src);
        return;
    }
    if (!pathFound) {
        printf("No path found\n");
    }
}

bool ValidateInput(const Graph* g)
{
    if (!g) {
        fprintf(stderr, "Error: NULL graph pointer.\n");
        return false;
    }

    for (int u = 0; u < g->vertices; u++) {          /* vertices */
        Node* cur = g->adjList[u];                    /* Node* */
        while (cur) {
            if (cur->weight < 0) {
                fprintf(stderr,
                    "Error: negative weight (%d) on edge %d->%d. "
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

void FreeGraphMemory(Graph* g)
{
    if (!g) return;

    for (int i = 0; i < g->vertices; i++) {          /* vertices */
        Node* cur = g->adjList[i];                    /* Node* */
        while (cur) {
            Node* next = cur->next;
            free(cur);
            cur = next;
        }
        g->adjList[i] = NULL;
    }

    free(g->adjList);
    g->adjList   = NULL;
    g->vertices  = 0;
}

void CleanupResources(NodeVisual* nodes, int* path)
{
    free(nodes);
    free(path);

    if (IsWindowReady()) {
        CloseWindow();
    }
}

/* ── Internal test helpers ────────────────────────────────────────────────── */

static Graph* build_test_graph(int numV, int edges[][3], int numEdges)
{
    Graph* g = (Graph*)malloc(sizeof(Graph));
    assert(g);
    g->vertices = numV;                                        /* vertices */
    g->adjList  = (Node**)calloc(numV, sizeof(Node*));        /* Node** */
    assert(g->adjList);

    for (int e = 0; e < numEdges; e++) {
        int u = edges[e][0], v = edges[e][1], w = edges[e][2];
        Node* node = (Node*)malloc(sizeof(Node));              /* Node* */
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

    bool valid = ValidateInput(g);
    assert(valid && "test graph must be valid");

    int dist[128], prev[128];
    dijkstra(g, src, dist, prev);   /* dijkstra (lowercase) */

    int path[128], pathSize = 0;
    if (dist[dst] == INF) {
        HandleSpecialCases(src, dst, 0);
        assert(!expectReachable && "expected unreachable");
    } else if (src == dst) {
        HandleSpecialCases(src, dst, 1);
        assert(expectWeight == 0);
    } else {
        int cur = dst;
        while (cur != -1) {
            path[pathSize++] = cur;
            cur = prev[cur];
        }
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

void RunSystemTests(void)
{
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║       Running System Tests           ║\n");
    printf("╚══════════════════════════════════════╝\n");

    {
        int edges[][3] = { {0,1,1}, {0,2,4}, {1,3,2}, {2,3,1} };
        run_test("T1 – Normal shortest path",
                 4, edges, 4, 0, 3, 1, 3);
    }
    {
        int edges[][3] = { {0,1,5} };
        run_test("T2 – Source equals destination",
                 2, edges, 1, 0, 0, 1, 0);
    }
    {
        Graph* g = build_test_graph(2, NULL, 0);
        printf("\n[TEST] T3 – No path exists\n");
        int dist[2], prev[2];
        dijkstra(g, 0, dist, prev);
        HandleSpecialCases(0, 1, dist[1] == INF ? 0 : 1);
        assert(dist[1] == INF && "node 1 must be unreachable");
        FreeGraphMemory(g);
        free(g);
        printf("[PASS] T3 – No path exists\n");
    }
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 4 – Milestone 3: Entity Path Animation
 * ═══════════════════════════════════════════════════════════════════════════ */

#define STEP_INTERVAL  0.3f   /* seconds between segment advances on an edge */
#define NODE_WAIT_TIME 1.0f   /* seconds to pause at each intermediate node  */
#define ENTITY_RADIUS  14.0f  /* visual radius of the moving circle          */

/* ── A. InitEntityAnimation ─────────────────────────────────────────────────
 *  Copies the Dijkstra path into the struct, places the entity on the first
 *  node, and resets every timer / flag so the animation is ready to play.
 * ────────────────────────────────────────────────────────────────────────── */
void InitEntityAnimation(EntityAnimation*  anim,
                         const int*        path,
                         int               pathSize,
                         const NodeVisual* nodes)
{
    if (!anim || !path || pathSize <= 0 || !nodes) return;

    /* Copy path array */
    for (int i = 0; i < pathSize && i < MAX_NODES; i++)
        anim->path[i] = path[i];

    anim->pathSize  = pathSize;
    anim->pathIndex = 0;
    anim->currentStep   = 0;
    anim->timer         = 0.0f;
    anim->isMoving      = false;   /* user must press Play */
    anim->isWaiting     = false;
    anim->reachedTarget = false;

    /* Snap to the screen position of the first node */
    int firstNode    = path[0];
    anim->currentPos = (Vector2){ nodes[firstNode].x, nodes[firstNode].y };
}

/* ── B. UpdateEntityAnimation ───────────────────────────────────────────────
 *  Called every frame.  Drives a two-state machine:
 *    WAITING → entity pauses NODE_WAIT_TIME seconds at an intermediate node
 *    MOVING  → entity advances one segment every STEP_INTERVAL seconds
 *
 *  Edge weight W (from the Graph adjacency list) defines how many segments
 *  the edge is divided into; each segment takes STEP_INTERVAL seconds.
 * ────────────────────────────────────────────────────────────────────────── */
void UpdateEntityAnimation(EntityAnimation*  anim,
                           const Graph*      g,
                           const NodeVisual* nodes)
{
    /* ── 1. Guard ────────────────────────────────────────────────────────── */
    if (!anim->isMoving || anim->reachedTarget) return;

    /* ── 2. Accumulate delta time ────────────────────────────────────────── */
    anim->timer += GetFrameTime();

    /* ── 3. WAITING state ────────────────────────────────────────────────── */
    if (anim->isWaiting) {
        if (anim->timer >= NODE_WAIT_TIME) {
            anim->timer     = 0.0f;
            anim->isWaiting = false;   /* done waiting; start next edge */
        }
        return;   /* nothing else to do while waiting */
    }

    /* ── 4. Already at last node? ────────────────────────────────────────── */
    if (anim->pathIndex >= anim->pathSize - 1) {
        anim->reachedTarget = true;
        anim->currentPos    = (Vector2){
            nodes[anim->path[anim->pathSize - 1]].x,
            nodes[anim->path[anim->pathSize - 1]].y
        };
        return;
    }

    /* ── 5. Identify current edge and its weight W ───────────────────────── */
    int fromNode = anim->path[anim->pathIndex];
    int toNode   = anim->path[anim->pathIndex + 1];

    /* Walk the adjacency list of fromNode to find the weight to toNode */
    int W = 1;   /* safe default */
    Node* cur = g->adjList[fromNode];
    while (cur) {
        if (cur->dest == toNode) { W = cur->weight; break; }
        cur = cur->next;
    }
    if (W <= 0) W = 1;   /* guard against zero-weight edges */

    Vector2 from = { nodes[fromNode].x, nodes[fromNode].y };
    Vector2 to   = { nodes[toNode].x,   nodes[toNode].y   };

    /* ── 6. Advance one segment every STEP_INTERVAL seconds ─────────────── */
    if (anim->timer >= STEP_INTERVAL) {
        anim->timer -= STEP_INTERVAL;   /* keep sub-tick remainder */
        anim->currentStep++;

        /* ── 7. Finished all W segments on this edge? ───────────────────── */
        if (anim->currentStep >= W) {
            anim->currentStep = 0;
            anim->pathIndex++;

            /* ── 8. Reached the final destination? ─────────────────────── */
            if (anim->pathIndex >= anim->pathSize - 1) {
                anim->currentPos = (Vector2){
                    nodes[anim->path[anim->pathSize - 1]].x,
                    nodes[anim->path[anim->pathSize - 1]].y
                };
                anim->reachedTarget = true;
                return;
            }

            /* Snap to the node we just arrived at */
            int arrivedAt    = anim->path[anim->pathIndex];
            anim->currentPos = (Vector2){ nodes[arrivedAt].x, nodes[arrivedAt].y };

            /* Enter 1-second wait at this intermediate node */
            anim->isWaiting = true;
            anim->timer     = 0.0f;
            return;
        }
    }

    /* ── 9. Interpolate position along the current edge ─────────────────── */
    /*  t goes from 0.0 (at `from`) toward 1.0 (at `to`) as steps complete  */
    float t = (float)anim->currentStep / (float)W;
    anim->currentPos.x = from.x + t * (to.x - from.x);
    anim->currentPos.y = from.y + t * (to.y - from.y);
}

/* ── C. DrawEntity ──────────────────────────────────────────────────────────
 *  Renders the entity as a layered circle (glow → body → highlight) at
 *  currentPos, matching the dark aesthetic already used in RenderFrame().
 *  On completion it draws a "Destination Reached!" banner.
 * ────────────────────────────────────────────────────────────────────────── */
void DrawEntity(const EntityAnimation* anim)
{
    if (!anim) return;

    /* Outer glow ring */
    DrawCircleV(anim->currentPos,
                ENTITY_RADIUS + 5.0f,
                (Color){ 255, 165, 0, 80 });

    /* Main body — orange stands out against the dark background */
    DrawCircleV(anim->currentPos, ENTITY_RADIUS,
                (Color){ 255, 165, 0, 255 });

    /* Inner highlight */
    DrawCircleV(anim->currentPos, ENTITY_RADIUS * 0.40f,
                (Color){ 255, 255, 255, 120 });

    /* ── Completion banner ────────────────────────────────────────────────── */
    if (anim->reachedTarget) {
        const char* msg   = "Destination Reached!";
        int         fs    = 26;
        int         textW = MeasureText(msg, fs);
        int         scrW  = GetScreenWidth();
        int         scrH  = GetScreenHeight();
        int         bx    = (scrW - textW) / 2 - 14;
        int         by    = scrH - 58;

        /* Semi-transparent backdrop */
        DrawRectangle(bx, by, textW + 28, fs + 14,
                      (Color){ 20, 20, 30, 200 });
        DrawRectangleLinesEx(
            (Rectangle){ (float)bx, (float)by,
                         (float)(textW + 28), (float)(fs + 14) },
            1.5f, (Color){ 255, 165, 0, 200 });

        DrawText(msg, (scrW - textW) / 2, by + 7, fs,
                 (Color){ 80, 220, 100, 255 });
    }
}

/* ── D. TogglePlayStop ──────────────────────────────────────────────────────
 *  Flips isMoving.  Call this from your UI button handler.
 *  Has no effect once the entity has reached its target — call
 *  InitEntityAnimation() first to restart the animation.
 * ────────────────────────────────────────────────────────────────────────── */
void TogglePlayStop(EntityAnimation* anim)
{
    if (!anim || anim->reachedTarget) return;
    anim->isMoving = !anim->isMoving;
}
