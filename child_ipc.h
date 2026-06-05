/*
 * child_ipc.h  –  Milestone 5: Child Sender API
 *
 * Declares the interface used by each child process to:
 *   1. Set up its own perspective of the IPC pipe (close unused read end).
 *   2. Send structured StatusMessage packets to the parent non-blocking.
 *
 * Usage sequence (inside the child branch after fork()):
 *
 *   // 1. Close all read ends and all write ends except our own.
 *   if (init_child_ipc_side(my_index) != 0) { exit(EXIT_FAILURE); }
 *
 *   // 2. For each node arrived at during the Dijkstra traversal:
 *   send_status_to_parent(current_node, next_node, is_dest, false);
 *
 *   // 3. Before exiting:
 *   send_status_to_parent(last_node, -1, true, true);
 */

#ifndef CHILD_IPC_H
#define CHILD_IPC_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  init_child_ipc_side
 *
 *  Called once in the child process after fork(), before any travel logic.
 *
 *  Responsibilities:
 *    • Close the READ end of our own pipe  (child never reads from parent).
 *    • Close ALL ends (both read and write) of every OTHER child's pipe to
 *      avoid holding stray file descriptors that could prevent the parent
 *      from detecting EOF on those channels.
 *
 *  Parameters:
 *    child_index – the 0-based index assigned to this child (same value
 *                  used in init_ipc_infrastructure / read_status_from_child).
 *
 *  Returns:  0 on success
 *           -1 on failure (the process should exit immediately)
 * ═══════════════════════════════════════════════════════════════════════════ */
/*
 * child_ipc_set_total_children
 *
 * Must be called in the PARENT before fork() to inform child_ipc how many
 * pipe pairs exist in total.  The value is inherited by each child via fork()
 * and used in init_child_ipc_side() to close all fds not owned by this child.
 */
void child_ipc_set_total_children(int n);

int init_child_ipc_side(int child_index);

/* ═══════════════════════════════════════════════════════════════════════════
 *  send_status_to_parent
 *
 *  Build and atomically write one StatusMessage to the parent through the
 *  child's dedicated pipe write end.
 *
 *  The child's PID is fetched internally via getpid() and embedded in every
 *  message so the parent can correlate messages with its traveler table.
 *
 *  Parameters:
 *    current_node   – the node index the child just arrived at.
 *    next_node      – the next node the child will move to (-1 if none).
 *    is_destination – true if current_node is the journey's final goal.
 *    is_finished    – true if the child is about to exit (last message).
 *
 *  Returns:  0 on success
 *           -1 on write failure (errno is set)
 *
 *  Note: write() on a pipe is atomic for payloads ≤ PIPE_BUF bytes.
 *  sizeof(StatusMessage) is well under this limit, so messages are never
 *  split across multiple reads on the parent side.
 * ═══════════════════════════════════════════════════════════════════════════ */
int send_status_to_parent(int current_node, int next_node,
                           bool is_destination, bool is_finished);

/* ═══════════════════════════════════════════════════════════════════════════
 *  close_child_ipc_side
 *
 *  Close the write end of this child's pipe.
 *
 *  Should be called just before exit() so the parent's read() gets EOF
 *  promptly.  Also called automatically by init_child_ipc_side() on error.
 * ═══════════════════════════════════════════════════════════════════════════ */
void close_child_ipc_side(void);

#endif /* CHILD_IPC_H */