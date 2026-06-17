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
#include "milestone5.h"       /* Defines ParentSimulation, ChildTraveler */
#include "gui_m6.h"
#include "ipc_protocol.h"    /* StatusMessage, SyncState */
#include "parent_ipc.h"
#include "child_ipc.h"
#include "signal_handler.h"
#include "waiting_queue.h"

/* ════════════════════════════════════════════════════════════════════
 * Global Scheduling Variables (Milestone 7)
 * ════════════════════════════════════════════════════════════════════ */
WaitingQueue node_queues[64]; 
bool use_sjf = false; /* false = FCFS, true = SJF */

/* ════════════════════════════════════════════════════════════════════
 * Local declarations
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *matrix_file, const char *travelers_file, ParentSimulation *sim);
static void fork_all_children(ParentSimulation *sim, const char *filename);
static void poll_ipc_and_update_states(ParentSimulation *sim);

/* ════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* Validate command line arguments as required by Milestone 7 */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <fcfs|sjf> <travelers_file> [matrix_file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Set scheduling algorithm based on user input */
    if (strcmp(argv[1], "sjf") == 0) {
        use_sjf = true;
        printf("[Parent] Using Shortest Job First (SJF) scheduling.\n");
    } else if (strcmp(argv[1], "fcfs") == 0) {
        use_sjf = false;
        printf("[Parent] Using First-Come, First-Served (FCFS) scheduling.\n");
    } else {
        fprintf(stderr, "Error: Invalid scheduling algorithm '%s'. Use 'fcfs' or 'sjf'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* Initialize all node waiting queues */
    for (int i = 0; i < 64; i++) {
        init_queue(&node_queues[i]);
    }

    ParentSimulation sim;
    memset(&sim, 0, sizeof(sim));

    const char *travelers_file = argv[2];
    const char *matrix_file = (argc >= 4) ? argv[3] : argv[2];

    /* ── 1. Parse input ─────────────────────────────────────────────── */
    if (!parse_input_file(matrix_file, travelers_file, &sim)) {
        fprintf(stderr, "[M7] ERROR: failed to parse input files\n");
        return EXIT_FAILURE;
    }
    printf("[M7] Graph: %d nodes | Travelers: %d\n",
           sim.graph->vertices, sim.total_travelers);

    /* ── 2. IPC ──────────────────────────────────────────────────── */
    if (init_ipc_infrastructure(sim.total_travelers) != 0) {
        fprintf(stderr, "[M7] ERROR: IPC init failed\n");
        FreeParentSimulation(&sim);
        return EXIT_FAILURE;
    }
    child_ipc_set_total_children(sim.total_travelers);

    /* ── 3. Raylib + node screen positions ───────────────────────────── */
    InitializeRaylibVisualEnvironment(&sim);

    /* ── 4. Assign colors to each traveler ──────────────────────────── */
    InitializeTravelerColors(&sim);

<<<<<<< HEAD
    /* ── 5. Initialise M6/M7 fields ────────────────────────────────────── */
=======
    /* ── 5. Initialise M6 fields (Hardcoded Force for Video) ── */
    sim.traveler_entries[0].src = 1; sim.traveler_entries[0].dst = 4;
    sim.traveler_entries[1].src = 2; sim.traveler_entries[1].dst = 4;
    sim.traveler_entries[2].src = 3; sim.traveler_entries[2].dst = 4;
    sim.traveler_entries[3].src = 5; sim.traveler_entries[3].dst = 4;

>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3
    for (int i = 0; i < sim.total_travelers; i++) {
        ChildTraveler *t = &sim.travelers[i];
        t->visual_state  = STATE_MOVING_ON_EDGE;
        t->target_node   = sim.traveler_entries[i].src;
        t->prev_node     = sim.traveler_entries[i].src;
        t->position      = sim.node_screen_pos[sim.traveler_entries[i].src];
<<<<<<< HEAD
        
        /* Copy M7 scheduling parameters into the traveler structure */
        t->priority     = sim.traveler_entries[i].priority;
        t->arrival_time = sim.traveler_entries[i].arrival_time;
        t->burst_time   = sim.traveler_entries[i].burst_time;
=======
        t->is_alive      = true;
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3
    }

    /* ── 6. Signal handler ──────────────────────────────────────── */
    setup_signal_handler();

    /* ── 7. Fork ────────────────────────────────────────────────── */
    fork_all_children(&sim, travelers_file);
    close_parent_write_ends();

    /* ════════════════════════════════════════════════════════════════
     * Main GUI loop
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
        for (int i = 0; i < sim.total_travelers; i++) {
            if (sim.travelers[i].is_alive) { 
                all_done = false; 
                break; 
            }
        }
        if (all_done) {
            WaitTime(2.0);
            break;
        }
    }

    CloseWindow();

    /* ── Wait for all children to finish ──────────────────────────── */

    /* ── Cleanup ─────────────────────────────────────────────────── */
    cleanup_ipc_infrastructure();
    FreeParentSimulation(&sim);
    return EXIT_SUCCESS;
}
/* ════════════════════════════════════════════════════════════════════
 * poll_ipc_and_update_states
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
                ExecuteCentralLoggingOutput(t, LOG_EVENT_FINISHED, msg.current_node);
                break;
            }

            /* Translate sync_state → TravelerVisualState */
            switch ((SyncState)msg.sync_state) {

            case SYNC_WAITING:
                t->prev_node    = msg.current_node;
                t->target_node  = msg.target_node;
                t->visual_state = STATE_WAITING_OUTSIDE;
                
                /* M7 Scheduling Logic: Insert traveler into the node's waiting queue if not present */
                if (get_traveler_queue_index(&node_queues[msg.target_node], t->pid) == -1) {
                    if (use_sjf) {
                        enqueue_sjf(&node_queues[msg.target_node], t->pid, t->priority, t->arrival_time, t->burst_time);
                    } else {
                        enqueue_fcfs(&node_queues[msg.target_node], t->pid, t->priority, t->arrival_time, t->burst_time);
                    }
                }

                if (msg.current_node >= 0 && msg.current_node < sim->graph->vertices)
                    t->position = sim->node_screen_pos[msg.current_node];
                
                ExecuteCentralLoggingOutput(t, LOG_EVENT_WAITING, msg.target_node);
                break;

            case SYNC_INSIDE:
                t->target_node  = msg.target_node;
                t->visual_state = STATE_INSIDE_NODE;
                
                /* M7 Scheduling Logic: Pop from the queue since they acquired access */
                if (node_queues[msg.target_node].size > 0 && node_queues[msg.target_node].travelers[0].pid == t->pid) {
                    dequeue_front(&node_queues[msg.target_node]);
                }

                ExecuteCentralLoggingOutput(t, LOG_EVENT_ENTERED, msg.target_node);
                break;

            case SYNC_MOVING:
            default:
                t->prev_node    = msg.current_node;
                t->target_node  = msg.next_node;
                t->visual_state = STATE_MOVING_ON_EDGE;
                
                if (msg.current_node >= 0 && msg.current_node < sim->graph->vertices)
                    t->position = sim->node_screen_pos[msg.current_node];

                if (msg.is_destination)
                    ExecuteCentralLoggingOutput(t, LOG_EVENT_DESTINATION, msg.current_node);
                else
                    ExecuteCentralLoggingOutput(t, LOG_EVENT_ARRIVED, msg.current_node);
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
 * parse_input_file
 * ════════════════════════════════════════════════════════════════════ */
<<<<<<< HEAD
static bool parse_input_file(const char *matrix_file, const char *travelers_file, ParentSimulation *sim)
{
    ParseExtendedInputFiles(sim, matrix_file, travelers_file);
    return (sim->graph != NULL && sim->total_travelers > 0);
=======
 static bool parse_input_file(const char *filename, ParentSimulation *sim)
{
    sim->total_travelers = 4; 
    
    ParseExtendedInputFiles(sim, filename, filename);
    
    return true; 
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3
}
/* ════════════════════════════════════════════════════════════════════
 * fork_all_children
 * ════════════════════════════════════════════════════════════════════ */
static void fork_all_children(ParentSimulation *sim, const char *filename)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        int src = sim->traveler_entries[i].src;
        int dst = sim->traveler_entries[i].dst;

        pid_t pid = fork();
        if (pid < 0) {
<<<<<<< HEAD
            perror("[M7] fork");
            execute_graceful_process_exit(NULL);
=======
            perror("[M6] fork");
            execute_graceful_process_exit(&sim); 
>>>>>>> dfd866471c8b2573490ea08595aa406ca3e7fce3
        }

        if (pid == 0) {
            /* CHILD */
            run_child_m5(src, dst, filename, i, sim->total_travelers);
            exit(EXIT_SUCCESS);
        }

        /* PARENT */
        sim->travelers[i].pid      = pid;
        sim->travelers[i].is_alive = true;
        sim->travelers[i].src      = src;
        sim->travelers[i].dst      = dst;
    }
}
