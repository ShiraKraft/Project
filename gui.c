/*
 * gui.c  –  Milestones 2 & 3: GUI, System, QA, and Cyber-Animation
 */

#ifndef STEP_DELAY_MS
#  define STEP_DELAY_MS 300
#endif
#ifndef NODE_WAIT_MS
#  define NODE_WAIT_MS 1000
#endif

#include "gui.h"
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

bool    isAnimating      = false;
float   moveTimer        = 0.0f;
float   nodeTimer        = 0.0f;
int     currentPathIndex = 0;
Vector2 exploitPos       = { 0.0f, 0.0f };
bool    animationDone    = false;
bool    onNodePause      = false;

#define CLR_BG          ((Color){ 10,  14,  26, 255})
#define CLR_GRID        ((Color){ 20,  28,  48, 255})
#define CLR_SECURE      ((Color){ 30, 160,  80, 255})
#define CLR_COMPROMISED ((Color){220,  40,  40, 255})
#define CLR_CURRENT     ((Color){255, 160,   0, 255})
#define CLR_EDGE_NORM   ((Color){100, 140, 200, 150})
#define CLR_EDGE_PATH   ((Color){255,  80,  80, 220})
#define CLR_EXPLOIT     ((Color){  0, 240, 255, 255})
#define CLR_BTN_PLAY    ((Color){ 20, 180,  80, 255})
#define CLR_BTN_STOP    ((Color){200,  40,  40, 255})
#define CLR_OVERLAY_BG  ((Color){  0,   0,   0, 200})
#define CLR_OVERLAY_TXT ((Color){255,  50,  50, 255})

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 1 – GUI & Visualization
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
        float angle    = (float)i * (2.0f * PI / (float)numNodes) - (PI / 2.0f);
        nodes[i].x      = center.x + radius * cosf(angle);
        nodes[i].y      = center.y + radius * sinf(angle);
        nodes[i].radius = 24.0f;
        nodes[i].color  = CLR_SECURE;
    }
}

void DrawGraphNode(NodeVisual node)
{
    DrawCircle((int)node.x + 3, (int)node.y + 3, node.radius, (Color){ 0, 0, 0, 60 });
    DrawCircleV((Vector2){ node.x, node.y }, node.radius, node.color);
    DrawCircleLinesV((Vector2){ node.x, node.y }, node.radius, WHITE);
    int fontSize = 16;
    int textW    = MeasureText(node.label, fontSize);
    DrawText(node.label, (int)(node.x - textW/2), (int)(node.y - fontSize/2), fontSize, WHITE);
}

void DrawDirectedEdge(Vector2 start, Vector2 end, int weight)
{
    Color edgeColor = CLR_EDGE_NORM;
    DrawLineV(start, end, edgeColor);

    float arrowLen = 14.0f, arrowAngle = 25.0f * DEG2RAD;
    float dx = end.x - start.x, dy = end.y - start.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1e-4f) return;
    float ux = -dx/len, uy = -dy/len;
    float cosA = cosf(arrowAngle), sinA = sinf(arrowAngle);
    Vector2 wing1 = { end.x + arrowLen*(ux*cosA - uy*sinA), end.y + arrowLen*(ux*sinA + uy*cosA) };
    Vector2 wing2 = { end.x + arrowLen*(ux*cosA + uy*sinA), end.y + arrowLen*(-ux*sinA + uy*cosA) };
    DrawLineV(end, wing1, edgeColor);
    DrawLineV(end, wing2, edgeColor);

    Vector2 mid = { (start.x+end.x)/2.0f, (start.y+end.y)/2.0f };
    char wLabel[16]; snprintf(wLabel, sizeof(wLabel), "%d", weight);
    int fontSize = 14, textW = MeasureText(wLabel, fontSize);
    DrawRectangle((int)(mid.x-textW/2-3),(int)(mid.y-fontSize/2-2),textW+6,fontSize+4,(Color){30,30,30,180});
    DrawText(wLabel,(int)(mid.x-textW/2),(int)(mid.y-fontSize/2),fontSize,YELLOW);
}

void RenderFrame(const Graph* g, const NodeVisual* nodes)
{
    if (!g || !nodes) return;
    BeginDrawing();
    ClearBackground(CLR_BG);
    for (int u = 0; u < g->vertices; u++) {
        Node* cur = g->adjList[u];
        while (cur) {
            int v = cur->dest;
            Vector2 from = { nodes[u].x, nodes[u].y };
            Vector2 to   = { nodes[v].x, nodes[v].y };
            float dx=to.x-from.x, dy=to.y-from.y, len=sqrtf(dx*dx+dy*dy);
            if (len > 1e-4f) { float r=nodes[v].radius; to.x-=(dx/len)*r; to.y-=(dy/len)*r; }
            DrawDirectedEdge(from, to, cur->weight);
            cur = cur->next;
        }
    }
    for (int i = 0; i < g->vertices; i++) DrawGraphNode(nodes[i]);
    DrawText("Graph Visualizer – Milestone 2", 10, 10, 18, (Color){180,180,180,255});
    DrawFPS(10, 34);
    EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 2 – System & Terminal Formatting
 * ═══════════════════════════════════════════════════════════════════════════ */

void PrintFinalPath(const int* path, int pathSize, int totalWeight)
{
    if (!path || pathSize <= 0) { printf("No path found\n"); return; }
    for (int i = 0; i < pathSize; i++) { printf("%d", path[i]); if (i < pathSize-1) printf(" -> "); }
    printf("\n%d\n", totalWeight);
}

void HandleSpecialCases(int src, int dst, int pathFound)
{
    if (src == dst) { printf("%d\n0\n", src); return; }
    if (!pathFound)  printf("No path found\n");
}

bool ValidateInput(const Graph* g)
{
    if (!g) { fprintf(stderr, "Error: NULL graph pointer.\n"); return false; }
    for (int u = 0; u < g->vertices; u++) {
        Node* cur = g->adjList[u];
        while (cur) {
            if (cur->weight < 0) { fprintf(stderr,"Error: negative weight (%d) on edge %d->%d.\n",cur->weight,u,cur->dest); return false; }
            cur = cur->next;
        }
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 3 – QA & Memory Management
 * ═══════════════════════════════════════════════════════════════════════════ */

void FreeGraphMemory(Graph* g)
{
    if (!g) return;
    for (int i = 0; i < g->vertices; i++) {
        Node* cur = g->adjList[i];
        while (cur) { Node* nxt = cur->next; free(cur); cur = nxt; }
        g->adjList[i] = NULL;
    }
    free(g->adjList); g->adjList = NULL; g->vertices = 0;
}

void CleanupResources(NodeVisual* nodes, int* path)
{
    free(nodes); free(path);
    if (IsWindowReady()) CloseWindow();
}

static Graph* build_test_graph(int numV, int edges[][3], int numEdges)
{
    Graph* g = (Graph*)malloc(sizeof(Graph)); assert(g);
    g->vertices = numV;
    g->adjList  = (Node**)calloc(numV, sizeof(Node*)); assert(g->adjList);
    for (int e = 0; e < numEdges; e++) {
        int u=edges[e][0], v=edges[e][1], w=edges[e][2];
        Node* node = (Node*)malloc(sizeof(Node)); assert(node);
        node->dest=v; node->weight=w; node->next=g->adjList[u]; g->adjList[u]=node;
    }
    return g;
}

static void run_test(const char* name, int numV, int edges[][3], int numEdges,
                     int src, int dst, int expectReachable, int expectWeight)
{
    printf("\n[TEST] %s\n", name);
    Graph* g = build_test_graph(numV, edges, numEdges);
    bool valid = ValidateInput(g); assert(valid);
    int dist[128], prev[128];
    dijkstra(g, src, dist, prev);
    int path[128], pathSize = 0;
    if (dist[dst] == INF) {
        HandleSpecialCases(src, dst, 0);
        assert(!expectReachable);
    } else if (src == dst) {
        HandleSpecialCases(src, dst, 1); assert(expectWeight == 0);
    } else {
        int cur = dst;
        while (cur != -1) { path[pathSize++] = cur; cur = prev[cur]; }
        for (int l=0,r=pathSize-1; l<r; l++,r--) { int tmp=path[l]; path[l]=path[r]; path[r]=tmp; }
        PrintFinalPath(path, pathSize, dist[dst]);
        assert(expectReachable); assert(dist[dst] == expectWeight);
    }
    FreeGraphMemory(g); free(g);
    printf("[PASS] %s\n", name);
}

void RunSystemTests(void)
{
    printf("\n╔══════════════════════════════════════╗\n║       Running System Tests           ║\n╚══════════════════════════════════════╝\n");
    { int e[][3]={{0,1,1},{0,2,4},{1,3,2},{2,3,1}}; run_test("T1 – Normal shortest path",4,e,4,0,3,1,3); }
    { int e[][3]={{0,1,5}}; run_test("T2 – Source equals destination",2,e,1,0,0,1,0); }
    {
        Graph* g = build_test_graph(2, NULL, 0);
        printf("\n[TEST] T3 – No path exists\n");
        int dist[2], prev[2]; dijkstra(g, 0, dist, prev);
        HandleSpecialCases(0, 1, dist[1]==INF?0:1);
        assert(dist[1]==INF); FreeGraphMemory(g); free(g);
        printf("[PASS] T3 – No path exists\n");
    }
    {
        printf("\n[TEST] T4 – Negative weight detection\n");
        int e[][3]={{0,1,-3},{1,2,2}}; Graph* g=build_test_graph(3,e,2);
        bool valid=ValidateInput(g); assert(!valid);
        FreeGraphMemory(g); free(g);
        printf("[PASS] T4 – Negative weight correctly rejected\n");
    }
    printf("\n╔══════════════════════════════════════╗\n║     All 4 system tests passed ✓      ║\n╚══════════════════════════════════════╝\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SECTION 4 – Milestone 3: Cyber Animation
 * ═══════════════════════════════════════════════════════════════════════════ */

static int EdgeWeight(const Graph* g, int u, int v)
{
    Node* cur = g->adjList[u];
    while (cur) { if (cur->dest == v) return cur->weight; cur = cur->next; }
    return 1;
}

static bool IsIntermediate(int idx, int pathLen)
{
    return idx > 0 && idx < pathLen - 1;
}

static bool IsCompromised(int nodeId, const PathResult* result)
{
    for (int i = 0; i <= currentPathIndex && i < result->length; i++)
        if (result->path[i] == nodeId) return true;
    return false;
}

bool GetDijkstraPathData(const Graph* g, const NodeVisual* nodes,
                         int src, int dst, PathResult* outPath)
{
    if (!g || !nodes || !outPath) return false;

    CompactGraph cg;
    init_graph(&cg, g->vertices);

    for (int u = 0; u < g->vertices; u++) {
        Node* cur = g->adjList[u];
        while (cur) {
            cg.edges[cg.edge_count] = (Edge){ cur->dest, cur->weight, cg.head[u] };
            cg.head[u] = cg.edge_count++;
            cur = cur->next;
        }
    }

    *outPath = run_dijkstra(&cg, src, dst);

    if (!outPath->found) return false;

    currentPathIndex = 0;
    moveTimer        = 0.0f;
    nodeTimer        = 0.0f;
    onNodePause      = false;
    animationDone    = false;
    isAnimating      = false;
    exploitPos       = (Vector2){ nodes[src].x, nodes[src].y };

    return true;
}

void UpdateAttackAnimation(const Graph* g, const NodeVisual* nodes,
                           const PathResult* result)
{
    if (!isAnimating || animationDone) return;
    if (!g || !nodes || !result || !result->found) return;

    float dt = GetFrameTime();

    if (onNodePause) {
        nodeTimer += dt;
        if (nodeTimer >= NODE_WAIT_MS / 1000.0f) {
            onNodePause = false; nodeTimer = 0.0f;
            currentPathIndex++; moveTimer = 0.0f;
        }
        return;
    }

    if (currentPathIndex >= result->length - 1) {
        animationDone = true; isAnimating = false; return;
    }

    int u = result->path[currentPathIndex];
    int v = result->path[currentPathIndex + 1];
    int W = EdgeWeight(g, u, v);
    float edgeDuration = (float)W * (STEP_DELAY_MS / 1000.0f);

    moveTimer += dt;
    float t = moveTimer / edgeDuration;
    if (t > 1.0f) t = 1.0f;

    exploitPos.x = nodes[u].x + t * (nodes[v].x - nodes[u].x);
    exploitPos.y = nodes[u].y + t * (nodes[v].y - nodes[u].y);

    if (t >= 1.0f) {
        exploitPos = (Vector2){ nodes[v].x, nodes[v].y };
        if (currentPathIndex + 1 >= result->length - 1) {
            animationDone = true; isAnimating = false;
            currentPathIndex = result->length - 1;
        } else {
            if (IsIntermediate(currentPathIndex + 1, result->length)) {
                onNodePause = true; nodeTimer = 0.0f;
            } else {
                currentPathIndex++; moveTimer = 0.0f;
            }
        }
    }
}

static bool IsPathEdge(int u, int v, const PathResult* result){
    for (int i = 0; i < currentPathIndex; i++)
        if (result->path[i] == u && result->path[i+1] == v) 
            return true;
    return false;
}

void DrawCyberGUI(const Graph* g, const NodeVisual* nodes,
                  const PathResult* result, Rectangle btnRect)
{
    if (!g || !nodes || !result) return;
    BeginDrawing();
    ClearBackground(CLR_BG);

    int screenW = GetScreenWidth(), screenH = GetScreenHeight();
    for (int x = 0; x < screenW; x += 40) DrawLine(x,0,x,screenH,CLR_GRID);
    for (int y = 0; y < screenH; y += 40) DrawLine(0,y,screenW,y,CLR_GRID);

    for (int u = 0; u < g->vertices; u++) {
        Node* cur = g->adjList[u];
        while (cur) {
            int v = cur->dest;
            Vector2 from={nodes[u].x,nodes[u].y}, to={nodes[v].x,nodes[v].y};
            float dx=to.x-from.x, dy=to.y-from.y, len=sqrtf(dx*dx+dy*dy);
            if (len > 1e-4f) { float r=nodes[v].radius; to.x-=(dx/len)*r; to.y-=(dy/len)*r; }
            if (result->found && IsPathEdge(u, v, result))
                DrawLineEx(from, to, 3.0f, CLR_EDGE_PATH);
            else
                DrawDirectedEdge(from, to, cur->weight);
            cur = cur->next;
        }
    }

    for (int i = 0; i < g->vertices; i++) {
        NodeVisual nv = nodes[i];
        if (result->found) {
            bool compromised = IsCompromised(i, result);
            bool isCurrent   = (currentPathIndex < result->length && result->path[currentPathIndex] == i);
            if      (isCurrent && isAnimating) nv.color = CLR_CURRENT;
            else if (compromised)              nv.color = CLR_COMPROMISED;
            else                               nv.color = CLR_SECURE;
        } else { nv.color = CLR_SECURE; }
        if (nv.color.r > 150 && nv.color.g < 100)
            DrawCircleV((Vector2){nv.x,nv.y}, nv.radius+6, (Color){nv.color.r,nv.color.g,nv.color.b,60});
        DrawGraphNode(nv);
    }

    if (result->found && (isAnimating || onNodePause || animationDone)) {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        float glowR = 18.0f + pulse * 6.0f;
        DrawCircleV(exploitPos, glowR+4, (Color){CLR_EXPLOIT.r,CLR_EXPLOIT.g,CLR_EXPLOIT.b,40});
        DrawCircleV(exploitPos, glowR,   (Color){CLR_EXPLOIT.r,CLR_EXPLOIT.g,CLR_EXPLOIT.b,90});
        DrawCircleV(exploitPos, 10.0f, CLR_EXPLOIT);
        DrawLineV((Vector2){exploitPos.x-16,exploitPos.y},(Vector2){exploitPos.x-12,exploitPos.y},CLR_EXPLOIT);
        DrawLineV((Vector2){exploitPos.x+12,exploitPos.y},(Vector2){exploitPos.x+16,exploitPos.y},CLR_EXPLOIT);
        DrawLineV((Vector2){exploitPos.x,exploitPos.y-16},(Vector2){exploitPos.x,exploitPos.y-12},CLR_EXPLOIT);
        DrawLineV((Vector2){exploitPos.x,exploitPos.y+12},(Vector2){exploitPos.x,exploitPos.y+16},CLR_EXPLOIT);
        DrawText("EXPLOIT",(int)(exploitPos.x-MeasureText("EXPLOIT",10)/2),(int)(exploitPos.y+14),10,CLR_EXPLOIT);
    }

    Color btnColor = isAnimating ? CLR_BTN_STOP : CLR_BTN_PLAY;
    DrawRectangleRec(btnRect, btnColor);
    DrawRectangleLinesEx(btnRect, 2, WHITE);
    const char* btnLabel = isAnimating ? "[ ABORT ]" : "[ START BREACH ]";
    int btnFontSize=16, btnTextW=MeasureText(btnLabel,btnFontSize);
    DrawText(btnLabel,(int)(btnRect.x+btnRect.width/2-btnTextW/2),(int)(btnRect.y+btnRect.height/2-btnFontSize/2),btnFontSize,WHITE);

    DrawText("NETWORK INTRUSION SIMULATOR – Milestone 3", 10, 10, 18, (Color){0,220,255,255});
    DrawFPS(10, 34);
    if (result->found) {
        char info[128];
        snprintf(info,sizeof(info),"Path cost: %d  |  Nodes: %d  |  Step: %d / %d",
                 result->total_weight, result->length, currentPathIndex, result->length-1);
        DrawText(info, 10, 58, 14, (Color){180,200,220,255});
    }

    if (animationDone) {
        DrawRectangle(0,0,screenW,screenH,CLR_OVERLAY_BG);
        const char* msg1="SYSTEM COMPROMISED", *msg2="Target database breached.";
        int fs1=52,fs2=24;
        DrawText(msg1,screenW/2-MeasureText(msg1,fs1)/2,screenH/2-60,fs1,CLR_OVERLAY_TXT);
        DrawText(msg2,screenW/2-MeasureText(msg2,fs2)/2,screenH/2+10,fs2,(Color){255,120,120,255});
        char summary[64];
        snprintf(summary,sizeof(summary),"Total firewall complexity bypassed: %d",result->total_weight);
        int fs3=16;
        DrawText(summary,screenW/2-MeasureText(summary,fs3)/2,screenH/2+54,fs3,(Color){200,200,200,255});
    }
    EndDrawing();
}