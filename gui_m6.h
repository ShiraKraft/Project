/*
 * gui_m6.h  –  Milestone 6: Node Access Synchronization – GUI Header
 *
 * DROP-IN REPLACEMENT for gui.h  (or #include after gui.h using an
 * extension pattern; recommended: replace gui.h entirely with this file).
 *
 * CHANGES VS. MILESTONE 5
 * ───────────────────────
 *  1. TravelerVisualState enum  – three states required by the spec.
 *  2. ChildTraveler struct      – two new navigation fields + the state field.
 *  3. New function declarations – CalculateWaitingPosition,
 *                                 UpdateTravelersPositions (M6 overload),
 *                                 RenderSimulationFrame    (M6 overload).
 *
 * All existing symbols from gui.h / gui_parent.h are preserved unchanged
 * so that milestone4 / milestone5 translation units keep compiling.
 */

#ifndef GUI_M6_H
#define GUI_M6_H

/* ── keep existing headers ─────────────────────────────────────────────── */
#include "raylib.h"
#include "graph.h"
#include "ipc_protocol.h"
#include "parent_ipc.h"

#include <sys/types.h>
#include <stdbool.h>

/* ════════════════════════════════════════════════════════════════════════
 *  1. TravelerVisualState – Milestone 6 synchronisation states
 *
 *  Every ChildTraveler carries one of these values.  The parent sets
 *  it when it receives an IPC message whose sync_state field signals
 *  a transition (see ipc_protocol_m6.h for the extended message).
 * ════════════════════════════════════════════════════════════════════════ */
typedef enum {
    /*
     * STATE_MOVING_ON_EDGE
     * The traveler currently holds no node lock and is interpolating
     * its screen position linearly from its previous node toward its
     * target node.  This is the default state between node arrivals.
     */
    STATE_MOVING_ON_EDGE = 0,

    /*
     * STATE_WAITING_OUTSIDE
     * The traveler has arrived at the boundary of its target node but
     * the node mutex/semaphore is held by another process.  The traveler
     * is blocked (queued) outside the node.
     *
     * Visually: drawn with an ORANGE ring and a "WAIT" label at a
     * position offset from the node centre along the incoming edge.
     */
    STATE_WAITING_OUTSIDE = 1,

    /*
     * STATE_INSIDE_NODE
     * The traveler has successfully acquired the node lock and is
     * spending its mandatory 1-second dwell time inside the node.
     *
     * Visually: drawn as a solid circle directly at the node centre,
     * surrounded by a pulsing GREEN ring to distinguish it from
     * STATE_MOVING_ON_EDGE travelers that happen to be near a node.
     */
    STATE_INSIDE_NODE = 2

} TravelerVisualState;


/* ════════════════════════════════════════════════════════════════════════
 *  2. ChildTraveler – extended for Milestone 6
 *
 *  All Milestone 5 fields are preserved.  Three new fields are added:
 *    • visual_state   – current synchronisation state (see enum above)
 *    • target_node    – the node this traveler is currently heading to
 *                       (the node whose lock it wants / holds)
 *    • prev_node      – the node it departed from (used by
 *                       CalculateWaitingPosition to compute the
 *                       direction vector for the queue offset)
 * ════════════════════════════════════════════════════════════════════════ */
typedef struct {
    /* ── Milestone 5 fields (unchanged) ─────────────────────────── */
    pid_t   pid;
    Vector2 position;       /* current rendered screen position        */
    Color   color;          /* unique per-traveler display colour       */
    bool    is_alive;       /* false once is_finished received          */
    int     current_node;   /* last confirmed graph node                */
    int     next_node;      /* graph node being travelled toward (-1 at dest) */
    int     src;            /* journey source                           */
    int     dst;            /* journey destination                      */

    /* ── Milestone 6 additions ───────────────────────────────────── */
    TravelerVisualState visual_state; /* current sync state             */
    int     target_node;  /* node whose lock is wanted / held           */
    int     prev_node;    /* node departed from (for queue offset dir)  */
    int priority;
    int arrival_time;
    int burst_time;
} ChildTraveler;


/* ════════════════════════════════════════════════════════════════════════
 *  3. ParentSimulation – unchanged from Milestone 5 except ChildTraveler
 *     now carries the new fields above.  No structural changes needed here.
 * ════════════════════════════════════════════════════════════════════════ */
#define MAX_TRAVELERS   32
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720
#define TARGET_FPS      60

#define LOG_EVENT_ARRIVED     0
#define LOG_EVENT_DESTINATION 1
#define LOG_EVENT_FINISHED    2
/* New M6 log event types */
#define LOG_EVENT_WAITING     3   /* traveler blocked outside node */
#define LOG_EVENT_ENTERED     4   /* traveler acquired node lock   */

typedef struct {
    int src;
    int dst;
    int arrival_time;   /* time-step the traveler enters the simulation */
    int priority;       /* lower number = higher priority (for priority-based schedulers) */
    int burst_time;     /* expected "service time" at a node, used by SJF */
} TravelerEntry;

typedef struct {
    Graph          *graph;
    Vector2         node_screen_pos[64];   /* MAX_NODES */
    ChildTraveler   travelers[MAX_TRAVELERS];
    TravelerEntry   traveler_entries[MAX_TRAVELERS];
    int             total_travelers;
} ParentSimulation;


/* ════════════════════════════════════════════════════════════════════════
 *  4. Milestone 6 – New function declarations
 * ════════════════════════════════════════════════════════════════════════ */

/*
 * CalculateWaitingPosition
 * ────────────────────────
 * Computes the screen-space position at which a waiting traveler should
 * be rendered in the queue outside its locked target node.
 *
 * Algorithm (as specified):
 *   dir = normalise(prevNodeCenter − nodeCenter)   // points back along edge
 *   offset = nodeRadius + (waitingIndex * QUEUE_SPACING)
 *   *outPosition = nodeCenter + dir * offset
 *
 * This places travelers in a neat single-file line extending from the
 * node perimeter back along the incoming path.
 *
 * Parameters:
 *   nodeCenter     – screen centre of the locked (target) node
 *   prevNodeCenter – screen centre of the node the traveler came from
 *   waitingIndex   – 0-based queue position (0 = closest to node)
 *   nodeRadius     – visual radius of the node circle (pixels)
 *   outPosition    – output: computed screen position (must not be NULL)
 */
void CalculateWaitingPosition(Vector2 nodeCenter,
                              Vector2 prevNodeCenter,
                              int     waitingIndex,
                              float   nodeRadius,
                              Vector2 *outPosition);

/*
 * UpdateTravelersPositions
 * ────────────────────────
 * Updates the rendered screen position of every traveler in sim->travelers[]
 * according to its current TravelerVisualState.
 *
 * Must be called every frame, BEFORE RenderSimulationFrame().
 *
 * State handling:
 *
 *   STATE_MOVING_ON_EDGE:
 *     The traveler's position is already being advanced by the existing
 *     IPC-driven interpolation logic.  This function leaves position
 *     untouched; the caller (ImplementNonBlockingIPCMonitoring or the
 *     main loop) handles the lerp.
 *
 *   STATE_WAITING_OUTSIDE:
 *     Count how many other travelers share the same target_node and are
 *     also in STATE_WAITING_OUTSIDE; determine this traveler's queue index
 *     (0 = first in queue); call CalculateWaitingPosition() and assign the
 *     result to traveler->position.
 *
 *   STATE_INSIDE_NODE:
 *     Snap traveler->position directly to sim->node_screen_pos[target_node].
 *
 * Parameters:
 *   sim – master simulation state (travelers and node positions must
 *         already be populated)
 */
void UpdateTravelersPositions(ParentSimulation *sim);

/*
 * RenderSimulationFrame
 * ─────────────────────
 * Full per-frame render pass.  Must be called between BeginDrawing() /
 * EndDrawing().
 *
 * Drawing order:
 *   1. Background + grid
 *   2. Graph edges (directed arrows with weight labels)
 *   3. Graph nodes (circles with ID labels; locked nodes highlighted)
 *   4. Travelers
 *        – STATE_INSIDE_NODE   : normal circle at node centre +
 *                                pulsing GREEN outline ring
 *        – STATE_WAITING_OUTSIDE: circle with ORANGE fill +
 *                                 "WAIT" text label above the dot
 *        – STATE_MOVING_ON_EDGE : normal circle at interpolated position
 *   5. HUD: per-traveler legend (color swatch + state text + PIDs)
 *
 * Parameters:
 *   sim – master simulation state (const – no side effects)
 */
void RenderSimulationFrame(const ParentSimulation *sim);


/* ════════════════════════════════════════════════════════════════════════
 *  5. Existing Milestone 5 function declarations (unchanged)
 *     Re-declared here so callers only need to include this one header.
 * ════════════════════════════════════════════════════════════════════════ */
void ParseExtendedInputFiles(ParentSimulation *sim,
                             const char *matrix_file,
                             const char *travelers_file);
void InitializeRaylibVisualEnvironment(ParentSimulation *sim);
void InitializeTravelerColors(ParentSimulation *sim);
void ImplementNonBlockingIPCMonitoring(ParentSimulation *sim);
void RenderMultiColorTravelerIndicators(const ParentSimulation *sim);
void ExecuteCentralLoggingOutput(const ChildTraveler *updater,
                                 int event_type,
                                 int node_id);
void SynchronizeProcessTerminations(ParentSimulation *sim);
void FreeParentSimulation(ParentSimulation *sim);

#endif /* GUI_M6_H */