/*
 * milestone4.h  –  Milestone 4: Parent Process Architect & GUI Manager
 *
 * Defines the data structures and function prototypes for multi-traveler
 * simulation using fork()-based child processes and Raylib animation.
 *
 * The parent process owns the rendering loop and controls all child
 * process lifecycles. Each child "lives" for the duration of its journey
 * and is terminated via SIGUSR1 the moment the parent detects arrival.
 */                  

#ifndef MILESTONE4_H
#define MILESTONE4_H

#include <sys/types.h>   /* pid_t                      */
#include <stdbool.h>
#include "raylib.h"
#include "graph.h"       /* Graph, NodeVisual, MAX_NODES */
#include "gui.h"         /* NodeVisual                  */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAX_TRAVELERS      32      /* maximum simultaneous travelers          */
#define MAX_PATH_NODES     MAX_NODES

/* Animation timing – kept consistent with Milestone 3 values in gui.c */
#define M4_STEP_INTERVAL   0.3f   /* seconds per edge-weight unit            */
#define M4_NODE_WAIT_TIME  1.0f   /* seconds to pause at intermediate nodes  */
#define M4_ENTITY_RADIUS   14.0f  /* visual radius of the traveler circle    */

typedef struct {
    pid_t child_pid;
    int current_node;
    int next_node;
    int is_destination;
    int is_finished;
} IPC_Message;
/* ═══════════════════════════════════════════════════════════════════════════
 *  Traveler
 *  Represents one traveler / child-process pair managed by the parent.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    /* ── Process identity ───────────────────────────────────────────────── */
    pid_t pid;              /* PID assigned after a successful fork()        */
int read_fd;
    /* ── Journey definition ─────────────────────────────────────────────── */
    int src_node;           /* source node index (from input file)           */
    int dest_node;          /* destination node index (from input file)      */
    int path[MAX_PATH_NODES]; /* ordered node indices computed by Dijkstra   */
    int path_size;          /* valid entries in path[]                       */

    /* ── Animation state ────────────────────────────────────────────────── */
    int     current_path_index; /* index into path[] of the node we left last */
    int     current_step;       /* how many weight-unit segments completed    */
    Vector2 current_pos;        /* screen position updated each frame         */
    float   move_timer;         /* accumulates GetFrameTime() between steps   */
    float   node_timer;         /* accumulates time while on_node_pause==true */
    bool    on_node_pause;      /* true while waiting 1 s at an intermediate  */

    /* ── Visual identity ────────────────────────────────────────────────── */
    Color color;            /* unique, vivid Raylib color for this traveler  */

    /* ── Lifecycle flag ─────────────────────────────────────────────────── */
    bool is_active;         /* false after arrival + child termination        */
} Traveler;

/* ═══════════════════════════════════════════════════════════════════════════
 *  SimulationManager
 *  Container that the parent process operates on throughout the session.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    Traveler travelers[MAX_TRAVELERS];
    int      traveler_count;         /* number of valid entries in travelers[] */
} SimulationManager;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Process Control Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * OrchestrateChildProcesses – fork() one child per traveler.
 *
 * For each traveler in sim->travelers[]:
 *   • In the child (pid==0): call ExecuteChildLogic() then exit(0).
 *   • In the parent: record the child PID and mark the traveler active.
 *
 * Child processes run concurrently.  The parent returns immediately after
 * all fork() calls complete and never blocks inside this function.
 */
void OrchestrateChildProcesses(SimulationManager* sim);

/*
 * TerminateChildProcess – send SIGUSR1 to the child identified by childPid.
 *
 * Called by the parent as soon as the traveler's on-screen animation reaches
 * its destination node.  SIGUSR1 is the agreed-upon termination signal
 * (matches the run_child() signal handler in child.c from Milestone 3).
 *
 * Falls back silently if childPid <= 0 (child already gone / never forked).
 */
void TerminateChildProcess(pid_t childPid);

/*
 * WaitForAllChildren – blocking cleanup after the rendering loop exits.
 *
 * Iterates sim->travelers[] and calls waitpid() for every traveler that
 * was ever assigned a positive PID, consuming their exit status to prevent
 * zombie processes.  Any child that is still running when this is called
 * (e.g. the window was closed early) is killed first via SIGUSR1 so that
 * waitpid() does not block indefinitely.
 */
void WaitForAllChildren(SimulationManager* sim);

/* ═══════════════════════════════════════════════════════════════════════════
 *  GUI & Animation Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * InitializeTravelerColors – assign a distinct color to each traveler.
 *
 * Uses a fixed palette of highly separable Raylib colors so that travelers
 * are visually distinguishable on the dark graph background even when paths
 * overlap.
 */
void InitializeTravelerColors(SimulationManager* sim);

/*
 * UpdateAllTravelersAnimation – advance every active traveler by dt seconds.
 *
 * For each active Traveler:
 *   • If on_node_pause: accumulate node_timer; release after M4_NODE_WAIT_TIME.
 *   • Otherwise:        accumulate move_timer; advance one weight-unit segment
 *                       every M4_STEP_INTERVAL seconds, interpolating
 *                       current_pos linearly between the two adjacent nodes.
 *   • On arrival at the final node: call TerminateChildProcess() and set
 *     is_active = false so it stops drawing and being updated.
 *
 * Parameters:
 *   sim   – the simulation container
 *   g     – the adjacency-list Graph (used to look up edge weights)
 *   nodes – the NodeVisual array (screen coordinates for each graph node)
 *   dt    – delta time in seconds for this frame (from GetFrameTime())
 */
void UpdateAllTravelersAnimation(SimulationManager*  sim,
                                 const Graph*        g,
                                 const NodeVisual*   nodes,
                                 float               dt);

/*
 * DrawActiveTravelers – render all active travelers inside a Raylib draw call.
 *
 * Must be called between BeginDrawing() and EndDrawing().
 * Each traveler is drawn as a layered circle (glow ring + body + highlight)
 * in its unique color, with a small PID label drawn above the circle.
 */
void DrawActiveTravelers(const SimulationManager* sim);

/*
 * ManagementGuiLoop – the top-level Raylib rendering loop for Milestone 4.
 *
 * Responsibilities:
 *   1. Accumulate frame time and call UpdateAllTravelersAnimation() each tick.
 *   2. BeginDrawing() / ClearBackground() / draw static graph / draw travelers
 *      / EndDrawing() on every frame.
 *   3. Exit early when all travelers become inactive (all journeys complete).
 *   4. Call WaitForAllChildren() after the loop to reap any remaining children.
 *
 * Parameters:
 *   sim   – the fully-initialized simulation (paths pre-computed, children
 *           already forked via OrchestrateChildProcesses)
 *   g     – the adjacency-list Graph
 *   nodes – the NodeVisual array (layout pre-computed via
 *           CalculateCircularLayout from gui.c)
 */
void ManagementGuiLoop(SimulationManager* sim,
                       const Graph*       g,
                       const NodeVisual*  nodes);

/* ─── Child-process stub (implemented in child.c) ─────────────────────────
 *  The parent only calls OrchestrateChildProcesses; child.c's run_child()
 *  is used instead.  ExecuteChildLogic is kept as a forward declaration so
 *  this header can document the contract clearly.
 *
 *  void ExecuteChildLogic(Traveler* t);
 */

#endif /* MILESTONE4_H */