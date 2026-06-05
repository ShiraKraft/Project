/*
 * ipc_protocol.h  –  Milestone 5: IPC Message Protocol Definition
 *
 * Defines the canonical wire format for status messages transmitted
 * from each autonomous child process to the parent's rendering loop.
 *
 * Transport layer: one UNIX pipe per child (O_NONBLOCK on the read end).
 * Messages are fixed-size binary structs; sizeof(StatusMessage) is always
 * the atomic unit exchanged in a single write()/read() call.
 *
 * Thread/process safety: write() on a pipe is atomic for payloads up to
 * PIPE_BUF bytes (≥ 512 B on Linux; typically 4096 B).  sizeof(StatusMessage)
 * is well below this limit, so a single write() is guaranteed to be either
 * completely written or not written at all – no torn reads.
 */

#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <sys/types.h>   /* pid_t    */
#include <stdbool.h>     /* bool     */

/* ═══════════════════════════════════════════════════════════════════════════
 *  StatusMessage  –  the single canonical IPC payload
 *
 *  Packed with __attribute__((packed)) so that the struct layout is
 *  identical on both sides of the pipe regardless of compiler padding.
 *  All integer fields use fixed-width platform types (pid_t is typedef'd
 *  to int on all supported Linux/POSIX targets).
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct __attribute__((packed)) {

    /*
     * pid  –  PID of the transmitting child process.
     *
     * Populated automatically by send_status_to_parent() via getpid().
     * The parent uses this to cross-reference messages against its
     * traveler table when the pipe index alone is insufficient (e.g.
     * for logging and error diagnostics).
     */
    pid_t pid;

    /*
     * current_node  –  index of the graph node the child just arrived at.
     *
     * The parent uses this to update the traveler's visual position and
     * to print the "[PID] arrived at node X | next node: Y" log line.
     */
    int current_node;

    /*
     * next_node  –  index of the next graph node the child will travel to.
     *
     * Set to -1 when is_destination is true (no next hop exists).
     * The parent displays this in the terminal log.
     */
    int next_node;

    /*
     * is_destination  –  true when current_node is the child's final goal.
     *
     * The parent prints "| DESTINATION" instead of "| next node: Y"
     * when this flag is set, then updates the GUI to show the traveler
     * has stopped moving.
     */
    bool is_destination;

    /*
     * is_finished  –  true when the child is about to call exit().
     *
     * Sent as the very last message before the child process terminates.
     * Lets the parent know it can close its write-end descriptor and stop
     * polling this child's pipe channel.
     * The parent prints "[PID] finished" upon receiving this flag.
     */
    bool is_finished;

} StatusMessage;

#endif /* IPC_PROTOCOL_H */