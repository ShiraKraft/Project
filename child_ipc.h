/*
 * child_ipc.h  –  Milestone 6: Child Sender API (Extended)
 *
 * Changes from Milestone 5:
 *   Added send_status_m6() which also fills sync_state and target_node.
 *   send_status_to_parent() is kept for backward compatibility with
 *   milestone 4/5 code; it calls send_status_m6() with SYNC_MOVING.
 */

#ifndef CHILD_IPC_H
#define CHILD_IPC_H

#include <stdbool.h>
#include "ipc_protocol.h"   /* SyncState */

/* Set the total number of children before fork() */
void child_ipc_set_total_children(int n);

/* Initialise the child side of IPC (close unused file descriptors) */
int init_child_ipc_side(int child_index);

/*
 * send_status_to_parent – Milestone 5 API (backward compatible)
 * Sends a message with sync_state=SYNC_MOVING and target_node=next_node.
 */
int send_status_to_parent(int current_node, int next_node,
                           bool is_destination, bool is_finished);

/*
 * send_status_m6 – Milestone 6 API
 * Sends a full StatusMessage including sync_state and target_node.
 */
int send_status_m6(int current_node, int next_node,
                   bool is_destination, bool is_finished,
                   SyncState sync_state, int target_node);

/* Close the write end of this child's pipe */
void close_child_ipc_side(void);

#endif /* CHILD_IPC_H */