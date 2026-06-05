/*
 * gui_parent.h  –  Milestone 5: Parent Process & GUI Architect
 *
 * Defines the master data structures and declares all functions
 * responsible for:
 *   • Parsing extended input files (graph + travelers)
 *   • Initializing the Raylib visual environment
 *   • Non-blocking IPC monitoring from child pipes
 *   • Rendering multi-color traveler indicators
 *   • Central logging output
 *   • Synchronizing process terminations
 */

#ifndef GUI_PARENT_H
#define GUI_PARENT_H

#define _POSIX_C_SOURCE 200809L

#include "raylib.h"
#include "graph.h"
#include "ipc_protocol.h"
#include "parent_ipc.h"

#include <sys/types.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Constants
 * ═══════════════════════════════════════════════════════════════════ */
#define MAX_TRAVELERS   32
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720
#define TARGET_FPS      60

/* Log event types – used by ExecuteCentralLoggingOutput */
#define LOG_EVENT_ARRIVED     0   /* traveler arrived at a node          */
#define LOG_EVENT_DESTINATION 1   /* traveler reached its destination    */
#define LOG_EVENT_FINISHED    2   /* child process called exit()         */

/* ═══════════════════════════════════════════════════════════════════
 *  ChildTraveler
 *  Tracks a single autonomous child process and its visual state.
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    pid_t   pid;          /* PID of the child process                  */
    Vector2 position;     /* current screen-space (x, y) position      */
    Color   color;        /* unique display color for this traveler     */
    bool    is_alive;     /* false once is_finished flag received       */
    int     current_node; /* last known graph node index                */
    int     next_node;    /* next graph node index (-1 at destination)  */
    int     src;          /* source node (from travelers file)          */
    int     dst;          /* destination node (from travelers file)     */
} ChildTraveler;

/* ═══════════════════════════════════════════════════════════════════
 *  TravelerEntry
 *  Raw data read from the travelers input file.
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    int src;
    int dst;
} TravelerEntry;

/* ═══════════════════════════════════════════════════════════════════
 *  ParentSimulation
 *  Master state structure for the entire parent engine.
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    Graph          *graph;                       /* loaded map structure              */
    Vector2         node_screen_pos[MAX_NODES];  /* pre-calculated screen positions   */
    ChildTraveler   travelers[MAX_TRAVELERS];     /* one entry per child process       */
    TravelerEntry   traveler_entries[MAX_TRAVELERS]; /* src/dst from file             */
    int             total_travelers;             /* parsed from travelers file        */
} ParentSimulation;

/* ═══════════════════════════════════════════════════════════════════
 *  Function declarations
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * ParseExtendedInputFiles
 * Task 1: Parse the graph matrix file and the travelers file.
 *
 * Fills sim->graph, sim->total_travelers, and sim->traveler_entries[].
 *
 * Travelers file format (one line per traveler):
 *   <total_travelers>
 *   <src0> <dst0>
 *   <src1> <dst1>
 *   ...
 *
 * Parameters:
 *   sim           – pointer to the master simulation state (must not be NULL)
 *   matrix_file   – path to the graph adjacency/weight file
 *   travelers_file– path to the travelers source/destination file
 */
void ParseExtendedInputFiles(ParentSimulation *sim,
                             const char *matrix_file,
                             const char *travelers_file);

/*
 * InitializeRaylibVisualEnvironment
 * Task 2: Open the Raylib window and pre-calculate node screen positions.
 *
 * Calls InitWindow() + SetTargetFPS(TARGET_FPS).
 * Maps every graph node to a circular layout inside the window.
 *
 * Parameters:
 *   sim – master simulation state (graph must already be loaded)
 */
void InitializeRaylibVisualEnvironment(ParentSimulation *sim);

/*
 * InitializeTravelerColors
 * Assign a unique, visually distinct color to each traveler slot.
 * Call this once after ParseExtendedInputFiles and before forking.
 *
 * Parameters:
 *   sim – master simulation state
 */
void InitializeTravelerColors(ParentSimulation *sim);

/*
 * ImplementNonBlockingIPCMonitoring
 * Task 3: Poll every child pipe for StatusMessage updates.
 *
 * Must be called every frame inside the Raylib loop.
 * Never blocks – all reads use O_NONBLOCK via read_status_from_child().
 * Updates sim->travelers[i].position / current_node / is_alive.
 * Calls ExecuteCentralLoggingOutput() on new messages.
 *
 * Parameters:
 *   sim – master simulation state
 */
void ImplementNonBlockingIPCMonitoring(ParentSimulation *sim);

/*
 * RenderMultiColorTravelerIndicators
 * Task 4: Draw the static graph and each live traveler on screen.
 *
 * Must be called between BeginDrawing() / EndDrawing().
 *
 * Parameters:
 *   sim – master simulation state (const – no modifications)
 */
void RenderMultiColorTravelerIndicators(const ParentSimulation *sim);

/*
 * ExecuteCentralLoggingOutput
 * Task 5: Print a formatted log line to stdout for a live IPC event.
 *
 * The parent is the ONLY process allowed to print logs.
 * Always flushes stdout after printing.
 *
 * Parameters:
 *   updater    – the ChildTraveler whose state just changed
 *   event_type – LOG_EVENT_ARRIVED / LOG_EVENT_DESTINATION / LOG_EVENT_FINISHED
 *   node_id    – the node index relevant to this event
 */
void ExecuteCentralLoggingOutput(const ChildTraveler *updater,
                                 int event_type,
                                 int node_id);

/*
 * SynchronizeProcessTerminations
 * Task 6: Block until every child process exits.
 *
 * Uses waitpid() on every registered PID.
 * Prevents orphan processes after the GUI window closes.
 *
 * Parameters:
 *   sim – master simulation state
 */
void SynchronizeProcessTerminations(ParentSimulation *sim);

/*
 * FreeParentSimulation
 * Release all heap memory held by the simulation (graph, etc.).
 *
 * Parameters:
 *   sim – master simulation state
 */
void FreeParentSimulation(ParentSimulation *sim);

#endif /* GUI_PARENT_H */
