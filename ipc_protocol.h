/*
 * ipc_protocol.h  –  Milestone 6: IPC Message Protocol (Extended)
 *
 * Changes from Milestone 5:
 *   Added enum SyncState and two new fields to StatusMessage:
 *     sync_state  – synchronisation state (MOVING / WAITING / INSIDE)
 *     target_node – the node relevant to the current sync_state
 */

#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <sys/types.h>
#include <stdbool.h>

/* ── Synchronisation states ─────────────────────────────────────── */
typedef enum {
    SYNC_MOVING  = 0,  /* traversing an edge, holding no lock          */
    SYNC_WAITING = 1,  /* blocked outside a locked node (before sem_wait) */
    SYNC_INSIDE  = 2   /* inside the node, holding the lock (after sem_wait) */
} SyncState;

/* ── IPC payload ─────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {

    /* Milestone 5 fields – unchanged */
    pid_t pid;
    int   current_node;
    int   next_node;
    bool  is_destination;
    bool  is_finished;

    /* New Milestone 6 fields */
    int sync_state;   /* value from the SyncState enum above */
    int target_node;  /* node the traveler is waiting for / currently inside */

} StatusMessage;

#endif /* IPC_PROTOCOL_H */