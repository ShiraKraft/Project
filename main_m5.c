/*
 * main_m5.c  –  Milestone 5: Parent Process & GUI Architect – Entry Point
 *
 * Responsibilities:
 *   1. Parse the single input file (graph + travelers).
 *   2. Initialize IPC infrastructure (pipes, one per child).
 *   3. Initialize Raylib visual environment.
 *   4. Assign unique colors to each traveler.
 *   5. Register SIGINT handler (Milestone 6 integration).
 *   6. Fork one child per traveler (each runs run_child_m5).
 *   7. Close parent write ends of all pipes.
 *   8. Run the main GUI loop:
 *        • Poll IPC for status updates (non-blocking).
 *        • Render the graph + traveler indicators.
 *        • Check termination flag / window-close.
 *   9. Synchronize process terminations (waitpid all children).
 *  10. Clean up all resources and exit.
 *
 * Input file format (single file):
 *   <num_nodes> <num_edges>
 *   <src> <dst> <weight>      (repeated num_edges times)
 *   <num_travelers>
 *   <src_node> <dst_node>     (repeated num_travelers times)
 *
 * Build:
 *   make milestone5
 *
 * Run:
 *   ./sim <input_file>
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
#include "graph.h"        /* Graph, initGraph, addEdge, freeGraph, dijkstra */
#include "gui_parent.h"   /* ParentSimulation, all Task 1-6 functions        */
#include "parent_ipc.h"   /* init_ipc_infrastructure, close_parent_write_ends,
                             cleanup_ipc_infrastructure                       */
#include "child_ipc.h"    /* child_ipc_set_total_children                    */
#include "milestone5.h"   /* run_child_m5                                    */
#include "signal_handler.h" /* setup_signal_handler, termination_requested,
                               execute_graceful_process_exit                  */

/* ═══════════════════════════════════════════════════════════════════
 *  parse_input_file
 *
 *  Reads the single combined input file into *sim.
 *  Fills sim->graph, sim->total_travelers, sim->traveler_entries[],
 *  and pre-fills sim->travelers[i].src / .dst / .current_node.
 *
 *  This mirrors the logic in main_m4.c but routes through
 *  ParseExtendedInputFiles so that all parsing is owned by the
 *  Parent Process component (gui_parent.c, Task 1).
 *
 *  Because the Milestone 5 input is one unified file (graph header +
 *  edges + traveler count + traveler src/dst lines), we split it
 *  into the two logical halves that ParseExtendedInputFiles expects:
 *  we pass the same filename for both arguments and let
 *  ParseExtendedInputFiles handle the sequential parse internally.
 *
 *  Returns true on success, false on any error.
 * ═══════════════════════════════════════════════════════════════════ */
static bool parse_input_file(const char *filename, ParentSimulation *sim)
{
    /*
     * ParseExtendedInputFiles (gui_parent.c) opens the file once and
     * reads graph header + edges, then continues reading the traveler
     * section from the same file pointer.  We therefore pass the same
     * path for both the "matrix_file" and "travelers_file" arguments –
     * the implementation handles the unified format.
     */
    ParseExtendedInputFiles(sim, filename, filename);
    return (sim->graph != NULL && sim->total_travelers > 0);
}

/* ═══════════════════════════════════════════════════════════════════
 *  fork_all_children
 *
 *  For each traveler i:
 *    • In the child  (pid == 0): call run_child_m5() – never returns.
 *    • In the parent           : record the PID, mark is_alive = true.
 *
 *  Must be called AFTER init_ipc_infrastructure() and
 *  child_ipc_set_total_children() so that every child inherits the
 *  full pipe table.
 * ═══════════════════════════════════════════════════════════════════ */
static void fork_all_children(ParentSimulation *sim, const char *filename)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        int src = sim->traveler_entries[i].src;
        int dst = sim->traveler_entries[i].dst;

        pid_t pid = fork();

        if (pid == -1) {
            perror("[main_m5] fork");
            /* Non-fatal for already-forked children; mark this slot dead */
            sim->travelers[i].pid      = -1;
            sim->travelers[i].is_alive = false;
            continue;
        }

        if (pid == 0) {
            /* ── Child branch ──────────────────────────────────────────
             * run_child_m5 reads the graph itself, runs Dijkstra,
             * travels node-by-node, sends StatusMessages, then exits.
             * It never returns.
             */
            run_child_m5(src, dst, filename, i, sim->total_travelers);
            /* unreachable */
            exit(EXIT_FAILURE);
        }

        /* ── Parent branch ─────────────────────────────────────────── */
        sim->travelers[i].pid      = pid;
        sim->travelers[i].is_alive = true;

        printf("[parent] forked child %d: PID %d  (node %d → %d)\n",
               i, (int)pid, src, dst);
        fflush(stdout);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  all_children_done
 *
 *  Returns true when every traveler slot has is_alive == false,
 *  meaning all children have sent their is_finished message.
 * ═══════════════════════════════════════════════════════════════════ */
static bool all_children_done(const ParentSimulation *sim)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        if (sim->travelers[i].is_alive) return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *input_file = argv[1];

    /* ── 1. Register SIGINT handler (Milestone 6) ─────────────────── */
    /*
     * Must be done before fork() so children inherit the registration.
     * The handler sets termination_requested = 1 asynchronously;
     * the GUI loop checks this flag every frame.
     */
    setup_signal_handler();

    /* ── 2. Allocate and zero-initialise the master simulation state ─ */
    ParentSimulation sim;
    memset(&sim, 0, sizeof(sim));

    /* ── 3. Parse input file (Task 1) ─────────────────────────────── */
    if (!parse_input_file(input_file, &sim)) {
        fprintf(stderr, "[main_m5] Failed to parse input file: %s\n",
                input_file);
        return EXIT_FAILURE;
    }
    printf("[parent] Graph: %d nodes | Travelers: %d\n",
           sim.graph->vertices, sim.total_travelers);
    fflush(stdout);

    /* ── 4. Initialize IPC pipes (one per child) ──────────────────── */
    if (init_ipc_infrastructure(sim.total_travelers) != 0) {
        fprintf(stderr, "[main_m5] IPC initialization failed.\n");
        FreeParentSimulation(&sim);
        return EXIT_FAILURE;
    }

    /*
     * Inform child_ipc how many pipe pairs exist so it can close all
     * fds it does not own after fork().  Must be called before fork().
     */
    child_ipc_set_total_children(sim.total_travelers);

    /* ── 5. Initialize Raylib window and node screen positions (Task 2) */
    InitializeRaylibVisualEnvironment(&sim);

    /* ── 6. Assign unique colors to each traveler ─────────────────── */
    InitializeTravelerColors(&sim);

    /* ── 7. Fork all child processes ──────────────────────────────── */
    fork_all_children(&sim, input_file);

    /*
     * Close the write ends of all pipes in the PARENT immediately
     * after fork().  If left open, the parent's read() will never
     * see EOF on any pipe (because the parent itself holds a write end),
     * causing cleanup to block forever.
     */
    close_parent_write_ends();

    /* ── 8. Main GUI loop ──────────────────────────────────────────── */
    /*
     * Every frame:
     *   a) Check for termination signal or window-close event.
     *   b) Poll all child pipes for StatusMessage updates (non-blocking).
     *   c) Render the graph + active traveler indicators.
     *   d) Break when all children have finished.
     */
    while (!WindowShouldClose() && !termination_requested) {

        /* (a) Non-blocking IPC poll – Task 3 */
        ImplementNonBlockingIPCMonitoring(&sim);

        /* (b) Render frame – Task 4 */
        BeginDrawing();
        RenderMultiColorTravelerIndicators(&sim);
        EndDrawing();

        /* (c) Exit loop early once every child has finished */
        if (all_children_done(&sim)) {
            printf("[parent] All travelers reached their destinations.\n");
            fflush(stdout);
            break;
        }
    }

    /* ── 9. Graceful exit ─────────────────────────────────────────── */
    /*
     * If we got here because of SIGINT or window-close, hand off to
     * the Milestone 6 teardown sequence which:
     *   • Sends SIGUSR1 to any still-alive children.
     *   • Calls waitpid() on all children.
     *   • Releases IPC and heap memory.
     *   • Calls exit(0).
     *
     * If we got here because all children finished naturally, we call
     * SynchronizeProcessTerminations (Task 6) to do a clean waitpid
     * pass, then clean up manually.
     */
    if (termination_requested) {
        /* Delegate full teardown to signal_handler.c */
        execute_graceful_process_exit(&sim);
        /* unreachable – execute_graceful_process_exit calls exit() */
    }

    /* Normal exit path ──────────────────────────────────────────── */

    /* Close Raylib window */
    if (IsWindowReady()) CloseWindow();

    /* Wait for all children (Task 6) */
    SynchronizeProcessTerminations(&sim);

    /* Release IPC file descriptors */
    cleanup_ipc_infrastructure();

    /* Free graph and simulation memory */
    FreeParentSimulation(&sim);

    printf("[parent] Simulation complete. Goodbye.\n");
    fflush(stdout);

    return EXIT_SUCCESS;
}
