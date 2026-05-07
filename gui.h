#ifndef GUI_H
#define GUI_H

#include "raylib.h"
#include "graph.h"
#include <stdbool.h>

/* ── NodeVisual ─────────────────────────────────────────────────────────── */
typedef struct {
    int   id;
    float x, y;
    float radius;
    Color color;
    char  label[32];
} NodeVisual;

/* ── 1. GUI & Visualization ─────────────────────────────────────────────── */
void InitGraphWindow(int width, int height, const char* title);
void CalculateCircularLayout(NodeVisual* nodes, int numNodes,
                             Vector2 center, float radius);
void DrawGraphNode(NodeVisual node);
void DrawDirectedEdge(Vector2 start, Vector2 end, int weight);
void RenderFrame(const Graph* g, const NodeVisual* nodes);

/* ── 2. System & Terminal Formatting ────────────────────────────────────── */
void PrintFinalPath(const int* path, int pathSize, int totalWeight);
void HandleSpecialCases(int src, int dst, int pathFound);
bool ValidateInput(const Graph* g);

/* ── 3. QA & Memory Management ──────────────────────────────────────────── */
void FreeGraphMemory(Graph* g);
void CleanupResources(NodeVisual* nodes, int* path);
void RunSystemTests(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestone 3 – Entity Animation
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── EntityAnimation ─────────────────────────────────────────────────────── */
typedef struct {
    int     path[MAX_NODES]; /* node-ID sequence from Dijkstra              */
    int     pathSize;        /* total nodes in path                         */
    Vector2 currentPos;      /* current screen position of the entity       */
    float   timer;           /* elapsed-time accumulator (fed GetFrameTime) */
    int     currentStep;     /* segment index within the active edge (0..W) */
    int     pathIndex;       /* which node in `path` we are leaving from    */
    bool    isMoving;        /* Play / Stop toggle                          */
    bool    isWaiting;       /* true while pausing 1 s at an intermediate   */
    bool    reachedTarget;   /* true once entity arrives at path[pathSize-1]*/
} EntityAnimation;

/* ── 4. Animation Functions ─────────────────────────────────────────────── */
void InitEntityAnimation  (EntityAnimation* anim,
                           const int* path, int pathSize,
                           const NodeVisual* nodes);

void UpdateEntityAnimation(EntityAnimation* anim,
                           const Graph* g,
                           const NodeVisual* nodes);

void DrawEntity           (const EntityAnimation* anim);

void TogglePlayStop       (EntityAnimation* anim);

#endif /* GUI_H */
