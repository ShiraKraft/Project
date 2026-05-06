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

#endif /* GUI_H */
