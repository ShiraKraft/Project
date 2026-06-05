/*
 * parent_ipc.h  –  Milestone 5: Parent Receiver API
 *
 * Declares the interface the parent process uses to initialise its pipe
 * array, poll for incoming status messages from each child, and tear
 * everything down cleanly on exit.
 *
 * Design contract
 * ───────────────
 * • The parent NEVER blocks inside these functions.  All read ends are
 *   configured O_NONBLOCK so that the Raylib GUI loop can call
 *   read_status_from_child() on every frame without stalling.
 *
 * • One pipe pair per child:  pipefd[child_index][0] = read (parent)
 *                              pipefd[child_index][1] = write (child,
 *                              closed in parent after fork).
 *
 * • The write end is closed in the parent immediately after fork() via
 *   close_parent_write_ends(); forgetting to do so means the parent's
 *   read() would never see EOF, blocking cleanup forever.
 */

#ifndef PARENT_IPC_H
#define PARENT_IPC_H

#include "ipc_protocol.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  init_ipc_infrastructure
 *
 *  Allocate and open one pipe per child, configure each read end as
 *  O_NONBLOCK, and store internal state for subsequent calls.
 *
 *  Must be called BEFORE fork() so that both parent and children inherit
 *  both ends of every pipe.
 *
 *  Parameters:
 *    num_children – number of child processes that will be forked.
 *                   Must be > 0 and ≤ MAX_TRAVELERS (defined in milestone4.h).
 *
 *  Returns:  0 on success
 *           -1 on failure (errno is set; partial pipes are cleaned up
 *              internally before returning)
 * ═══════════════════════════════════════════════════════════════════════════ */
int init_ipc_infrastructure(int num_children);

/* ═══════════════════════════════════════════════════════════════════════════
 *  close_parent_write_ends
 *
 *  Close every pipe's write end (index [1]) in the PARENT process.
 *
 *  MUST be called once in the parent immediately after all fork() calls
 *  complete.  If write ends are left open in the parent, a subsequent
 *  read() on that pipe will never return 0 (EOF) when the child exits,
 *  causing WaitForAllChildren / cleanup to block indefinitely.
 *
 *  Safe to call multiple times; already-closed descriptors are skipped.
 * ═══════════════════════════════════════════════════════════════════════════ */
void close_parent_write_ends(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  read_status_from_child
 *
 *  Non-blocking attempt to read exactly one StatusMessage from the pipe
 *  associated with the child at position child_index.
 *
 *  Parameters:
 *    child_index – 0-based index matching the order children were forked.
 *    out_msg     – caller-allocated buffer; filled on success (return == 1).
 *
 *  Returns:
 *     1  –  message successfully read; *out_msg is populated.
 *     0  –  no message available right now (EAGAIN / EWOULDBLOCK); try again
 *           on the next frame.
 *    -1  –  error or the child closed its write end (EOF/pipe broken);
 *           the caller should stop polling this index.
 *
 *  This function never blocks: the underlying read() fd has O_NONBLOCK set.
 * ═══════════════════════════════════════════════════════════════════════════ */
int read_status_from_child(int child_index, StatusMessage *out_msg);

/* ═══════════════════════════════════════════════════════════════════════════
 *  cleanup_ipc_infrastructure
 *
 *  Close all remaining open file descriptors (read ends) and free the
 *  internal pipe-descriptor array.
 *
 *  Safe to call from an atexit() handler or directly at the end of main().
 *  Idempotent: calling it twice is harmless.
 * ═══════════════════════════════════════════════════════════════════════════ */
void cleanup_ipc_infrastructure(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  get_child_write_fd
 *
 *  Returns the write-end file descriptor for child child_index.
 *  Called from child_ipc.c after fork() to obtain the correct fd to
 *  write status messages through.
 *
 *  Returns the fd (>= 0) on success, or -1 if child_index is out of range.
 * ═══════════════════════════════════════════════════════════════════════════ */
int get_child_write_fd(int child_index);

#endif /* PARENT_IPC_H */