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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Milestone 3 – Animation State
 *  All globals are declared here; defined once in gui.c.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * isAnimating – Play/Stop toggle for the attack simulation.
 *   true  → UpdateAttackAnimation() advances each frame
 *   false → animation is paused (or not yet started)
 */
extern bool isAnimating;

/*
 * moveTimer – Accumulates GetFrameTime() while the exploit travels an edge.
 *   Edge with weight W takes W * 300 ms total.
 *   Resets to 0.0f each time the exploit enters a new edge.
 *   Accesses: PathResult.path[currentPathIndex] and PathResult.path[currentPathIndex+1]
 */
extern float moveTimer;

/*
 * nodeTimer – Accumulates GetFrameTime() while the exploit waits at a node.
 *   Pauses for 1000 ms at every intermediate node (not src, not dst).
 *   Resets to 0.0f each time the exploit arrives at a new node.
 *   Accesses: PathResult.path[currentPathIndex], PathResult.length
 */
extern float nodeTimer;

/*
 * currentPathIndex – Index into PathResult.path[] for the exploit's position.
 *   Ranges 0 … PathResult.length-1.
 *   Incremented by UpdateAttackAnimation() when the exploit finishes an edge.
 *   Accesses: PathResult.path[], PathResult.length
 */
extern int currentPathIndex;

/*
 * exploitPos – Screen-space XY of the exploit icon each frame.
 *   Linearly interpolated between NodeVisual[u] and NodeVisual[v]
 *   using (moveTimer / (weight * 0.3f)) as t.
 *   Accesses: NodeVisual[u].x/.y, NodeVisual[v].x/.y
 */
extern Vector2 exploitPos;

/*
 * animationDone – Set true when exploit reaches PathResult.path[length-1].
 *   Triggers the "System Compromised" overlay.
 *   Accesses: PathResult.length
 */
extern bool animationDone;

/*
 * onNodePause – true while exploit waits at an intermediate node.
 *   Prevents moveTimer from advancing during the 1-second node pause.
 *   Accesses: PathResult.path[currentPathIndex], PathResult.length
 */
extern bool onNodePause;

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

/* ── 4. Milestone 3 – Cyber Animation ───────────────────────────────────── */

/*
 * GetDijkstraPathData – Integration bridge between Dijkstra.c and the animator.
 *
 * Builds a CompactGraph from the adjList-based Graph, runs run_dijkstra(),
 * and stores the PathResult.  Initialises animation globals to their start state.
 *
 * Accesses:
 *   Graph.adjList, Graph.vertices              (to build CompactGraph)
 *   PathResult.path[], .length, .total_weight, .found
 *   NodeVisual[i].x, NodeVisual[i].y          (sets exploitPos to src position)
 *
 * Returns true if a path was found and outPath is valid.
 */
bool GetDijkstraPathData(const Graph* g, const NodeVisual* nodes,
                         int src, int dst, PathResult* outPath);

/*
 * UpdateAttackAnimation – Advances the exploit one step per frame.
 *
 * Must be called every frame while isAnimating == true.
 * Uses GetFrameTime() for frame-rate independence.
 *
 * State machine:
 *   Phase A (onNodePause == true):
 *     nodeTimer += dt.  After 1000 ms → clear pause, advance currentPathIndex,
 *     reset moveTimer.
 *     Accesses: PathResult.path[currentPathIndex], PathResult.length
 *
 *   Phase B (traversing an edge):
 *     Weight W looked up via Graph.adjList for edge
 *       path[currentPathIndex] → path[currentPathIndex+1].
 *     moveTimer += dt;  t = clamp(moveTimer / (W * 0.3f), 0, 1).
 *     exploitPos = lerp(NodeVisual[u].pos, NodeVisual[v].pos, t).
 *     Accesses: PathResult.path[currentPathIndex/+1],
 *               NodeVisual[].x/.y, Graph.adjList
 *
 *   On arrival (t >= 1):
 *     Intermediate node → onNodePause = true, nodeTimer = 0.
 *     Destination       → animationDone = true, isAnimating = false.
 */
void UpdateAttackAnimation(const Graph* g, const NodeVisual* nodes,
                           const PathResult* result);

/*
 * DrawCyberGUI – Full render pass for Milestone 3.
 *
 * Draws static network graph, colours compromised/current nodes,
 * renders the glowing exploit icon, the Start-Breach / Abort button,
 * and the "SYSTEM COMPROMISED" overlay when animationDone == true.
 *
 * Accesses:
 *   Graph.adjList, Graph.vertices
 *   NodeVisual[i].x/.y/.radius/.color/.label
 *   PathResult.path[], PathResult.length
 *   exploitPos, isAnimating, animationDone, currentPathIndex
 */
void DrawCyberGUI(const Graph* g, const NodeVisual* nodes,
                  const PathResult* result, Rectangle btnRect);

#endif /* GUI_H */