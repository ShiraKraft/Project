/*
 * main_m7.c  –  Milestone 7: System Integration + Scheduling
 *
 * USAGE
 *   ./sim-schd fcfs  <travelers_file> [matrix_file]
 *   ./sim-schd sjf   <travelers_file> [matrix_file]
 *
 * CHANGES FROM main_m6.c
 * ──────────────────────
 *  1. Git conflict markers (<<<<<<< / ======= / >>>>>>>) removed; the
 *     HEAD branch is canonical throughout.
 *  2. #include "gui_m6_m7.h" added.
 *  3. SetSchedulerDisplayName() called once after argv[] is parsed.
 *  4. RenderSimulationFrame(&sim) replaced with
 *     RenderSimulationFrame_M7(&sim, node_queues) in the main loop.
 *  5. Orphan hardcoded traveler_entries block removed (was in the
 *     dfd866 branch only; HEAD already reads from the input file).
 *  6. Title string updated to "Milestone 7".
 *
 * All IPC, fork, signal, and waiting-queue logic is unchanged from M6.
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
#include "milestone5.h"       /* ParentSimulation, ChildTraveler          */
#include "gui_m6.h"           /* M6 GUI, TravelerVisualState, structs      */
#include "gui_m6_m7.h"        /* M7 GUI overlay (scheduler badge, queues)  */
#include "ipc_protocol.h"     /* StatusMessage, SyncState                  */
#include "parent_ipc.h"
#include "child_ipc.h"
#include "signal_handler.h"
#include "waiting_queue.h"
#include "scheduler.h"

/* ════════════════════════════════════════════════════════════════════
 * Global Scheduling Variables (Milestone 7)
 * ════════════════════════════════════════════════════════════════════ */

/* Per-node waiting queues (one per graph node, up to 64 nodes) */
WaitingQueue node_queues[64];

/* false → FCFS (default),  true → SJF */
bool use_sjf = false;

/* ════════════════════════════════════════════════════════════════════
 * Forward declarations (static helpers)
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *matrix_file,
                             const char *travelers_file,
                             ParentSimulation *sim);
static void fork_all_children(ParentSimulation *sim, const char *filename);
static void poll_ipc_and_update_states(ParentSimulation *sim);

/* ════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* ── Command-line validation ────────────────────────────────────── */
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <fcfs|sjf> <travelers_file> [matrix_file]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* ── Parse scheduling algorithm ─────────────────────────────────── */
    if (strcmp(argv[1], "sjf") == 0) {
        use_sjf = true;
        printf("[M7] Scheduling algorithm: Shortest Job First (SJF)\n");
        SetSchedulerDisplayName("SJF");
        init_scheduler(SCHED_SJF);
    } else if (strcmp(argv[1], "fcfs") == 0) {
        use_sjf = false;
        printf("[M7] Scheduling algorithm: First-Come, First-Served (FCFS)\n");
        SetSchedulerDisplayName("FCFS");
        init_scheduler(SCHED_FCFS);
    } else {
        fprintf(stderr,
                "[M7] ERROR: Unknown scheduler '%s'. Use 'fcfs' or 'sjf'.\n",
                argv[1]);
        return EXIT_FAILURE;
    }

    /* ── Initialise per-node waiting queues ─────────────────────────── */
    for (int i = 0; i < 64; i++)
        init_queue(&node_queues[i]);

    /* ── Build simulation state ─────────────────────────────────────── */
    ParentSimulation sim;
    memset(&sim, 0, sizeof(sim));

    const char *travelers_file = argv[2];
    const char *matrix_file    = (argc >= 4) ? argv[3] : argv[2];

    /* ── 1. Parse input ─────────────────────────────────────────────── */
    if (!parse_input_file(matrix_file, travelers_file, &sim)) {
        fprintf(stderr, "[M7] ERROR: failed to parse input files\n");
        destroy_scheduler();
        return EXIT_FAILURE;
    }
    printf("[M7] Graph: %d nodes | Travelers: %d\n",
           sim.graph->vertices, sim.total_travelers);

    /* ── 2. IPC infrastructure ──────────────────────────────────────── */
    if (init_ipc_infrastructure(sim.total_travelers) != 0) {
        fprintf(stderr, "[M7] ERROR: IPC init failed\n");
        FreeParentSimulation(&sim);
        destroy_scheduler();
        return EXIT_FAILURE;
    }
    child_ipc_set_total_children(sim.total_travelers);

    /* ── 3. Raylib + node screen positions ──────────────────────────── */
    InitializeRaylibVisualEnvironment(&sim);
    SetWindowTitle("Network Traffic Simulator – Milestone 7");

    /* ── 4. Assign colours to each traveler ─────────────────────────── */
    InitializeTravelerColors(&sim);

    /* ── 5. Initialise M7 traveler fields ───────────────────────────── */
    for (int i = 0; i < sim.total_travelers; i++) {
        ChildTraveler *t = &sim.travelers[i];
        t->visual_state  = STATE_MOVING_ON_EDGE;
        t->target_node   = sim.traveler_entries[i].src;
        t->prev_node     = sim.traveler_entries[i].src;
        t->position      = sim.node_screen_pos[sim.traveler_entries[i].src];
        t->priority      = sim.traveler_entries[i].priority;
        t->arrival_time  = sim.traveler_entries[i].arrival_time;
        t->burst_time    = sim.traveler_entries[i].burst_time;
    }

    /* ── 6. Signal handler ──────────────────────────────────────────── */
    setup_signal_handler();

    /* ── 7. Fork children ───────────────────────────────────────────── */
    fork_all_children(&sim, travelers_file);
    close_parent_write_ends();

    /* ════════════════════════════════════════════════════════════════
     * Main GUI loop
     * ════════════════════════════════════════════════════════════════ */
    while (!WindowShouldClose() && !termination_requested) {

        /* Read IPC messages; update traveler states + waiting queues */
        poll_ipc_and_update_states(&sim);

        /* Update screen positions (WAITING → offset, INSIDE → snap) */
        UpdateTravelersPositions(&sim);

        /*
         * Draw  –  M7 wrapper draws M6 frame first, then overlays:
         *   • "Scheduler Mode: FCFS / SJF" badge (top-right)
         *   • Per-node waiting-queue panel (right side)
         *   • Entry-transition flash rings
         */
        RenderSimulationFrame_M7(&sim, node_queues);

        /* Check for simulation completion */
        bool all_done = true;
        for (int i = 0; i < sim.total_travelers; i++) {
            if (sim.travelers[i].is_alive) { all_done = false; break; }
        }
        if (all_done) {
            WaitTime(2.0);   /* hold final frame for 2 s */
            break;
        }
    }

    CloseWindow();

    /* ── Wait for all children ──────────────────────────────────────── */
    /* SynchronizeProcessTerminations() is declared in gui_m6.h but has  */
    /* no definition in any .c file in the project; we inline it here.   */
    {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, 0)) > 0) {
            printf("[M7] Child pid=%d exited with status %d\n",
                   (int)pid, WEXITSTATUS(status));
        }
    }

    /* ── Cleanup ────────────────────────────────────────────────────── */
    cleanup_ipc_infrastructure();
    FreeParentSimulation(&sim);
    destroy_scheduler();

    return EXIT_SUCCESS;
}

/* ════════════════════════════════════════════════════════════════════
 * poll_ipc_and_update_states
 *
 * Reads all pending IPC messages from every child pipe.
 * On SYNC_WAITING: enqueues the traveler into the correct node queue
 *   using the algorithm selected at startup (FCFS or SJF).
 * On SYNC_INSIDE:  dequeues the front entry (the traveler that was
 *   granted access) to keep the queue consistent with reality.
 * ════════════════════════════════════════════════════════════════════ */
static void poll_ipc_and_update_states(ParentSimulation *sim)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        ChildTraveler *t = &sim->travelers[i];
        if (!t->is_alive) continue;

        StatusMessage msg;
        int rc;

        while ((rc = read_status_from_child(i, &msg)) == 1) {

            /* Update basic navigation fields */
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

            /* ── Translate sync_state → TravelerVisualState ─────────── */
            switch ((SyncState)msg.sync_state) {

            /* ── SYNC_WAITING: blocked outside a locked node ─────────── */
            case SYNC_WAITING:
                t->prev_node    = msg.current_node;
                t->target_node  = msg.target_node;
                t->visual_state = STATE_WAITING_OUTSIDE;

                /*
                 * M7 Scheduling Integration
                 * Only enqueue if this traveler is not already in the
                 * queue (duplicate SYNC_WAITING messages can arrive
                 * while the traveler is still blocked).
                 */
                if (get_traveler_queue_index(
                        &node_queues[msg.target_node], t->pid) == -1) {

                    if (use_sjf) {
                        enqueue_sjf(&node_queues[msg.target_node],
                                    t->pid,
                                    t->priority,
                                    t->arrival_time,
                                    t->burst_time);
                    } else {
                        enqueue_fcfs(&node_queues[msg.target_node],
                                     t->pid,
                                     t->priority,
                                     t->arrival_time,
                                     t->burst_time);
                    }

                    printf("[M7] Node %d queue (%s): traveler pid=%d burst=%d  → depth=%d\n",
                           msg.target_node,
                           use_sjf ? "SJF" : "FCFS",
                           (int)t->pid, t->burst_time,
                           node_queues[msg.target_node].size);
                }

                if (msg.current_node >= 0 &&
                    msg.current_node < sim->graph->vertices)
                    t->position = sim->node_screen_pos[msg.current_node];

                ExecuteCentralLoggingOutput(t, LOG_EVENT_WAITING,
                                            msg.target_node);
                break;

            /* ── SYNC_INSIDE: traveler acquired the node lock ─────────── */
            case SYNC_INSIDE:
                t->target_node  = msg.target_node;
                t->visual_state = STATE_INSIDE_NODE;

                /*
                 * Pop the front of the queue: the scheduler guarantees
                 * the front entry is the traveler that was unblocked.
                 */
                if (node_queues[msg.target_node].size > 0 &&
                    node_queues[msg.target_node].travelers[0].pid == t->pid) {
                    dequeue_front(&node_queues[msg.target_node]);
                    printf("[M7] Node %d: traveler pid=%d entered (queue depth now %d)\n",
                           msg.target_node, (int)t->pid,
                           node_queues[msg.target_node].size);
                }

                ExecuteCentralLoggingOutput(t, LOG_EVENT_ENTERED,
                                            msg.target_node);
                break;

            /* ── SYNC_MOVING (default): traveler is traversing an edge ── */
            case SYNC_MOVING:
            default:
                t->prev_node    = msg.current_node;
                t->target_node  = msg.next_node;
                t->visual_state = STATE_MOVING_ON_EDGE;

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

        /* Pipe closed without is_finished flag – mark as done */
        if (rc == -1 && t->is_alive) {
            t->is_alive     = false;
            t->visual_state = STATE_MOVING_ON_EDGE;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 * parse_input_file
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *matrix_file,
                              const char *travelers_file,
                              ParentSimulation *sim)
{
    ParseExtendedInputFiles(sim, matrix_file, travelers_file);
    return (sim->graph != NULL && sim->total_travelers > 0);
}

/* ════════════════════════════════════════════════════════════════════
 * fork_all_children
 * ════════════════════════════════════════════════════════════════════ */
static void fork_all_children(ParentSimulation *sim, const char *filename)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        int   src = sim->traveler_entries[i].src;
        int   dst = sim->traveler_entries[i].dst;
        pid_t pid = fork();

        if (pid < 0) {
            perror("[M7] fork");
            execute_graceful_process_exit(NULL);
        }

        if (pid == 0) {
            /* ── CHILD process ──────────────────────────────────────── */
            run_child_m5(src, dst, filename, i, sim->total_travelers);
            exit(EXIT_SUCCESS);
        }

        /* ── PARENT: record child metadata ─────────────────────────── */
        sim->travelers[i].pid      = pid;
        sim->travelers[i].is_alive = true;
        sim->travelers[i].src      = src;
        sim->travelers[i].dst      = dst;
        printf("[M7] Forked traveler %d: pid=%d  src=%d dst=%d burst=%d\n",
               i, (int)pid, src, dst, sim->traveler_entries[i].burst_time);
    }
}