/*
 * signal_handler.c  –  Milestone 6: Signal Handling & Graceful Exit Architect
 *
 * Implements:
 *   Task 1 – handle_sigint             : async-safe SIGINT callback
 *   Task 2 – setup_signal_handler      : POSIX sigaction registration
 *   Task 3 – execute_graceful_process_exit : controlled teardown sequence
 *
 * Design rules enforced throughout this file:
 *   • Signal handler (handle_sigint) touches ONLY the atomic flag.
 *     No printf, no malloc, no locks – strictly async-signal-safe.
 *   • All blocking / resource-releasing work happens in
 *     execute_graceful_process_exit(), called from normal process context.
 *   • Every system call that can fail is checked; perror() is used for
 *     diagnostics so the error message includes the OS reason string.
 */

#define _POSIX_C_SOURCE 200809L

#include "signal_handler.h"
#include "gui_parent.h"      /* ParentSimulation, ChildTraveler          */
#include "parent_ipc.h"      /* cleanup_ipc_infrastructure()             */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>          /* kill()                                   */
#include <signal.h>          /* sigaction, sigemptyset, SIGINT, SIGUSR1  */
#include <sys/types.h>
#include <sys/wait.h>        /* waitpid()                                */

/* Raylib – needed only for IsWindowReady() / CloseWindow() */
#include "raylib.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Global atomic termination flag  (definition – declared extern in .h)
 *
 *  Initialised to 0 (no termination requested).
 *  The ONLY writer is handle_sigint(); every other site is a reader.
 * ═══════════════════════════════════════════════════════════════════ */
volatile sig_atomic_t termination_requested = 0;

/* ═══════════════════════════════════════════════════════════════════
 *  Task 1 – handle_sigint
 * ═══════════════════════════════════════════════════════════════════ */
void handle_sigint(int sig)
{
    /*
     * Async-signal-safety contract:
     *   This function may be invoked at ANY point in the program's
     *   execution – even inside a library call.  The only operation
     *   that is unconditionally async-signal-safe here is writing to
     *   a volatile sig_atomic_t variable.
     *
     *   DO NOT add printf, fprintf, malloc, free, or any function
     *   that is not listed in POSIX's table of async-signal-safe
     *   functions (man 7 signal-safety).
     */
    (void)sig;                      /* suppress unused-parameter warning */
    termination_requested = 1;      /* atomic write – safe in handler    */
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 2 – setup_signal_handler
 * ═══════════════════════════════════════════════════════════════════ */
void setup_signal_handler(void)
{
    struct sigaction sa;

    /*
     * Zero the entire struct first.  On some platforms sa_flags,
     * sa_restorer, or padding bytes may be uninitialised otherwise,
     * which can cause unpredictable behaviour.
     */
    memset(&sa, 0, sizeof(sa));

    /*
     * Point the handler field at our custom async-safe callback.
     * We use sa_handler (not sa_sigaction) because we do not need
     * the extended siginfo_t context – the flag flip is sufficient.
     */
    sa.sa_handler = handle_sigint;

    /*
     * sigemptyset() clears the signal mask stored in sa_mask.
     * Result: no additional signals are blocked while handle_sigint
     * executes.  This keeps the handler as non-intrusive as possible
     * and avoids accidentally delaying other signals (e.g. SIGUSR1
     * that children send to themselves).
     */
    if (sigemptyset(&sa.sa_mask) == -1) {
        perror("[signal] sigemptyset");
        exit(EXIT_FAILURE);
    }

    /*
     * sa_flags = 0  (no SA_RESTART, no SA_SIGINFO, etc.)
     *
     * Not setting SA_RESTART is intentional: if the process is inside
     * a blocking call (e.g. waitpid, read) when SIGINT arrives, the
     * call returns -1 / EINTR, which the main loop can treat as a
     * prompt to check termination_requested and break cleanly.
     */
    sa.sa_flags = 0;

    /*
     * Bind the configuration to the OS kernel for SIGINT.
     * Third argument NULL means we do not save the previous handler.
     */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("[signal] sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }

    printf("[signal] SIGINT handler registered successfully.\n");
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 3 – execute_graceful_process_exit
 * ═══════════════════════════════════════════════════════════════════ */
void execute_graceful_process_exit(void *sim_ptr)
{
    /*
     * Cast the opaque pointer back to ParentSimulation.
     * We accept void* so that signal_handler.h does not have to
     * include gui_parent.h, keeping the header dependency minimal.
     */
    ParentSimulation *sim = (ParentSimulation *)sim_ptr;

    printf("\n[signal] Termination requested – starting graceful exit...\n");
    fflush(stdout);

    /* ── Step 1: Close the Raylib window ─────────────────────────── */
    /*
     * IsWindowReady() returns false if InitWindow() was never called
     * or if CloseWindow() was already invoked – safe to call at any
     * point in the teardown sequence.
     */
    if (IsWindowReady()) {
        CloseWindow();
        printf("[signal] Raylib window closed.\n");
        fflush(stdout);
    }

    /* ── Step 2: Signal every live child to terminate ────────────── */
    /*
     * Each child's run_child() installs a SIGUSR1 handler that sets
     * its own g_terminate flag, causing it to exit its travel loop
     * and call exit(EXIT_SUCCESS) cleanly (see child.c).
     *
     * We send the signal only to children whose PID is known and
     * whose is_alive flag is still true.
     */
    if (sim) {
        for (int i = 0; i < sim->total_travelers; i++) {
            ChildTraveler *t = &sim->travelers[i];
            if (t->is_alive && t->pid > 0) {
                if (kill(t->pid, SIGUSR1) == -1) {
                    perror("[signal] kill(SIGUSR1)");
                    /* non-fatal: child may have already exited */
                } else {
                    printf("[signal] Sent SIGUSR1 to child PID %d\n",
                           (int)t->pid);
                    fflush(stdout);
                }
            }
        }
    }

    /* ── Step 3: Wait for all children (no orphan processes) ─────── */
    /*
     * We call waitpid() on every registered PID.  If the child
     * already exited the call returns immediately; if it is still
     * running (e.g. sleeping inside usleep) it will be woken by the
     * SIGUSR1 sent in step 2 and exit shortly.
     *
     * Using the same loop logic as SynchronizeProcessTerminations()
     * (gui_parent.c) ensures consistent reaping behaviour regardless
     * of which shutdown path is taken.
     */
    if (sim) {
        for (int i = 0; i < sim->total_travelers; i++) {
            pid_t pid = sim->travelers[i].pid;
            if (pid <= 0) continue;

            int   status = 0;
            pid_t ret    = waitpid(pid, &status, 0);

            if (ret == -1) {
                perror("[signal] waitpid");
                continue;
            }

            if (WIFEXITED(status)) {
                printf("[signal] Child PID %d exited (code %d)\n",
                       (int)pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[signal] Child PID %d terminated by signal %d\n",
                       (int)pid, WTERMSIG(status));
            }
            fflush(stdout);
        }
    }

    /* ── Step 4: Release IPC infrastructure ──────────────────────── */
    /*
     * cleanup_ipc_infrastructure() closes all remaining pipe file
     * descriptors and frees the internal pipe-fd table.
     * It is idempotent – safe to call even if already cleaned up.
     */
    cleanup_ipc_infrastructure();
    printf("[signal] IPC infrastructure released.\n");
    fflush(stdout);

    /* ── Step 5: Free simulation heap memory ─────────────────────── */
    if (sim) {
        FreeParentSimulation(sim);
        printf("[signal] Simulation memory freed.\n");
        fflush(stdout);
    }

    /* ── Step 6: Final status line ───────────────────────────────── */
    printf("[signal] All resources released. Exiting cleanly.\n");
    fflush(stdout);

    /* ── Step 7: Exit with success code ──────────────────────────── */
    /*
     * exit(0) flushes stdio buffers, runs atexit() handlers, and
     * reports EXIT_SUCCESS to the shell.  Using exit() rather than
     * _exit() is safe here because we are in normal process context
     * (not inside a signal handler).
     */
    exit(0);
}
