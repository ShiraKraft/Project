/*
 * signal_handler.h  –  Milestone 6: Signal Handling & Graceful Exit Architect
 *
 * Declares the global atomic termination flag, and all functions responsible
 * for:
 *   Task 1 – handle_sigint             : async-safe SIGINT callback
 *   Task 2 – setup_signal_handler      : POSIX sigaction registration
 *   Task 3 – execute_graceful_process_exit : controlled teardown sequence
 */

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Global Atomic Flag
 *
 *  volatile  – prevents the compiler from caching the value in a
 *              register; every read goes directly to memory.
 *  sig_atomic_t – guaranteed by the C standard to be readable and
 *              writable atomically in an async-signal context.
 *
 *  Rule: the ONLY writes allowed inside a signal handler are
 *  assignments to volatile sig_atomic_t variables.
 * ═══════════════════════════════════════════════════════════════════ */
extern volatile sig_atomic_t termination_requested;

/* ═══════════════════════════════════════════════════════════════════
 *  Function declarations
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * handle_sigint
 * Task 1: Async-safe SIGINT callback.
 *
 * Registered via sigaction(); called by the OS whenever SIGINT arrives
 * (e.g. Ctrl+C). Sets termination_requested = 1 and returns immediately.
 * Must never call printf, malloc, or any other async-signal-unsafe function.
 *
 * Parameters:
 *   sig – signal number delivered by the OS (always SIGINT here;
 *         parameter kept to satisfy the sa_handler prototype).
 */
void handle_sigint(int sig);

/*
 * setup_signal_handler
 * Task 2: POSIX signal registration & mask configuration.
 *
 * Builds a struct sigaction, clears the signal mask with sigemptyset(),
 * points sa_handler at handle_sigint(), and registers the binding with
 * sigaction(SIGINT, ...). Calls perror() and exits on failure.
 *
 * Call once, before forking any child processes, so that the registration
 * is inherited by every child that does not override it.
 */
void setup_signal_handler(void);

/*
 * execute_graceful_process_exit
 * Task 3: Controlled resource release & final termination.
 *
 * Centralised teardown sequence invoked after the main loop detects
 * termination_requested == 1 or the Raylib window is closed.
 *
 * Sequence:
 *   1. Close the Raylib window (if still open).
 *   2. Send SIGUSR1 to every live child so each exits its travel loop.
 *   3. Wait for all children via waitpid() (no orphan processes).
 *   4. Release IPC infrastructure (pipe file descriptors).
 *   5. Free simulation heap memory.
 *   6. Print a final status line to stdout and flush.
 *   7. Call exit(0) to report clean termination to the shell.
 *
 * Parameters:
 *   sim – pointer to the master ParentSimulation state; may be NULL
 *         (all steps that need it are guarded).
 */
void execute_graceful_process_exit(void *sim);

#endif /* SIGNAL_HANDLER_H */
