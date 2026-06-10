/*
 * main_m6.c  –  Milestone 6: Node Access Synchronisation – Entry Point
 *
 * Changes from main_m5.c:
 *   • poll_ipc_and_update_states() reads the sync_state and target_node fields
 *     and translates them directly into TravelerVisualState.
 *   • Calls UpdateTravelersPositions() every frame to update queues.
 *   • Calls RenderSimulationFrame() instead of RenderMultiColorTravelerIndicators().
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "raylib.h"
#include "graph.h"
#include "gui_m6.h"
#include "ipc_protocol.h"    /* StatusMessage, SyncState */
#include "parent_ipc.h"
#include "child_ipc.h"
#include "milestone5.h"
#include "signal_handler.h"

/* ════════════════════════════════════════════════════════════════════
 *  Local declarations
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *filename, ParentSimulation *sim);
static void fork_all_children(ParentSimulation *sim, const char *filename);
static void poll_ipc_and_update_states(ParentSimulation *sim);

/* ════════════════════════════════════════════════════════════════════
 *  main
 * ════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    ParentSimulation sim;
    memset(&sim, 0, sizeof(sim));

    /* ── 1. Parse input ─────────────────────────────────────────────── */
    if (!parse_input_file(argv[1], &sim)) {
        fprintf(stderr, "[M6] ERROR: failed to parse '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }
    printf("[M6] Graph: %d nodes | Travelers: %d\n",
           sim.graph->vertices, sim.total_travelers);

    /* ── 2. IPC ──────────────────────────────────────────────────── */
    if (init_ipc_infrastructure(sim.total_travelers) != 0) {
        fprintf(stderr, "[M6] ERROR: IPC init failed\n");
        FreeParentSimulation(&sim);
        return EXIT_FAILURE;
    }
    child_ipc_set_total_children(sim.total_travelers);

    /* ── 3. Raylib + node screen positions ───────────────────────────── */
    InitializeRaylibVisualEnvironment(&sim);

    /* ── 4. Assign colors to each traveler ──────────────────────────── */
    InitializeTravelerColors(&sim);

    /* ── 5. Initialise M6 fields ────────────────────────────────────── */
    for (int i = 0; i < sim.total_travelers; i++) {
        ChildTraveler *t = &sim.travelers[i];
        t->visual_state  = STATE_MOVING_ON_EDGE;
        t->target_node   = sim.traveler_entries[i].src;
        t->prev_node     = sim.traveler_entries[i].src;
        t->position      = sim.node_screen_pos[sim.traveler_entries[i].src];
    }

    /* ── 6. Signal handler ──────────────────────────────────────── */
    setup_signal_handler();

    /* ── 7. Fork ────────────────────────────────────────────────── */
    fork_all_children(&sim, argv[1]);
    close_parent_write_ends();

    /* ════════════════════════════════════════════════════════════════
     *  Main GUI loop
     * ════════════════════════════════════════════════════════════════ */
    while (!WindowShouldClose() && !termination_requested) {

        /* Read IPC messages and update each traveler's visual_state */
        poll_ipc_and_update_states(&sim);

        /* Update screen positions according to state */
        UpdateTravelersPositions(&sim);

        /* Draw */
        BeginDrawing();
        RenderSimulationFrame(&sim);
        EndDrawing();

        /* Check whether all travelers have finished */
        bool all_done = true;
        for (int i = 0; i < sim.total_travelers; i++)
            if (sim.travelers[i].is_alive) { all_done = false; break; }
        if (all_done) {
            WaitTime(2.0);
            break;
        }
    }

    CloseWindow();

    /* ── Wait for all children to finish ──────────────────────────── */
    SynchronizeProcessTerminations(&sim);

    /* ── Cleanup ─────────────────────────────────────────────────── */
    cleanup_ipc_infrastructure();
    FreeParentSimulation(&sim);
    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════════
 *  poll_ipc_and_update_states
 *
 *  Drains all accumulated messages from every child and translates sync_state
 *  into TravelerVisualState accordingly:
 *
 *    SYNC_WAITING → STATE_WAITING_OUTSIDE
 *    SYNC_INSIDE  → STATE_INSIDE_NODE
 *    SYNC_MOVING  → STATE_MOVING_ON_EDGE
 * ════════════════════════════════════════════════════════════════════ */
static void poll_ipc_and_update_states(ParentSimulation *sim)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        ChildTraveler *t = &sim->travelers[i];
        if (!t->is_alive) continue;

        StatusMessage msg;
        int rc;

        while ((rc = read_status_from_child(i, &msg)) == 1) {

            /* Update basic fields */
            t->pid          = msg.pid;
            t->current_node = msg.current_node;
            t->next_node    = msg.next_node;

            if (msg.is_finished) {
                t->is_alive     = false;
                t->visual_state = STATE_MOVING_ON_EDGE;
                ExecuteCentralLoggingOutput(t, LOG_EVENT_FINISHED,
                                            msg.current_node);
                break;
            }

            /* Translate sync_state → TravelerVisualState */
            switch ((SyncState)msg.sync_state) {

            case SYNC_WAITING:
                /*
                 * The traveler is queued outside the node.
                 * prev_node  = the last node it was at (current_node).
                 * target_node = the node it is trying to enter.
                 */
                t->prev_node    = msg.current_node;
                t->target_node  = msg.target_node;
                t->visual_state = STATE_WAITING_OUTSIDE;
                /* Update position to the current node (base for computing the wait queue) */
                if (msg.current_node >= 0 &&
                    msg.current_node < sim->graph->vertices)
                    t->position = sim->node_screen_pos[msg.current_node];
                ExecuteCentralLoggingOutput(t, LOG_EVENT_WAITING,
                                            msg.target_node);
                break;

            case SYNC_INSIDE:
                /*
                 * The traveler has acquired the lock and is inside the node.
                 */
                t->target_node  = msg.target_node;
                t->visual_state = STATE_INSIDE_NODE;
                ExecuteCentralLoggingOutput(t, LOG_EVENT_ENTERED,
                                            msg.target_node);
                break;

            case SYNC_MOVING:
            default:
                /*
                 * The traveler is moving along an edge / has released the lock.
                 */
                t->prev_node    = msg.current_node;
                t->target_node  = msg.next_node;
                t->visual_state = STATE_MOVING_ON_EDGE;
                /* Update screen position to the current node */
                if (msg.current_node >= 0 &&
                    msg.current_node < sim->graph->vertices)
                    t->position = sim->node_screen_pos[msg.current_node];

                if (msg.is_destination)
                    ExecuteCentralLoggingOutput(t, LOG_EVENT_DESTINATION,
                                                msg.current_node);
                else
                    ExecuteCentralLoggingOutput(t, LOG_EVENT_ARRIVED,
                                                msg.current_node);
                break;
            }
        }

        /* Pipe closed without is_finished – handle gracefully */
        if (rc == -1 && t->is_alive) {
            t->is_alive     = false;
            t->visual_state = STATE_MOVING_ON_EDGE;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  parse_input_file
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *filename, ParentSimulation *sim)
{
    ParseExtendedInputFiles(sim, filename, filename);
    return (sim->graph != NULL && sim->total_travelers > 0);
}

/* ════════════════════════════════════════════════════════════════════
 *  fork_all_children
 * ════════════════════════════════════════════════════════════════════ */
static void fork_all_children(ParentSimulation *sim, const char *filename)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        int src = sim->traveler_entries[i].src;
        int dst = sim->traveler_entries[i].dst;

        pid_t pid = fork();
        if (pid < 0) {
            perror("[M6] fork");
            execute_graceful_process_exit();
        }

        if (pid == 0) {
            /* CHILD */
            run_child_m5(src, dst, filename, i, sim->total_travelers);
            exit(EXIT_SUCCESS); /* should never be reached */
        }

        /* PARENT */
        sim->travelers[i].pid      = pid;
        sim->travelers[i].is_alive = true;
        sim->travelers[i].src      = src;
        sim->travelers[i].dst      = dst;
    }
}