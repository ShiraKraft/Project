/*
 * child_ipc.c  –  Milestone 6: Child Sender API Implementation
 *
 * Changes from Milestone 5:
 *   • send_status_to_parent() now delegates to send_status_m6()
 *     with sync_state=SYNC_MOVING (full backward compatibility).
 *   • send_status_m6() – new function that fills all StatusMessage fields.
 *
 * The rest of the code (init_child_ipc_side, close_child_ipc_side)
 * is identical to Milestone 5.
 */

#define _POSIX_C_SOURCE 200809L

#include "child_ipc.h"
#include "parent_ipc.h"
#include "ipc_protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ── Module state ────────────────────────────────────────────────── */
static int g_write_fd       = -1;
static int g_child_index    = -1;
static int g_total_children =  0;

/* ════════════════════════════════════════════════════════════════════
 *  child_ipc_set_total_children
 * ════════════════════════════════════════════════════════════════════ */
void child_ipc_set_total_children(int n)
{
    g_total_children = n;
}

/* ════════════════════════════════════════════════════════════════════
 *  init_child_ipc_side  – identical to Milestone 5
 * ════════════════════════════════════════════════════════════════════ */
int init_child_ipc_side(int child_index)
{
    if (child_index < 0 || g_total_children <= 0) {
        fprintf(stderr,
            "[child_ipc] init: invalid child_index=%d total=%d\n",
            child_index, g_total_children);
        return -1;
    }

    g_child_index = child_index;
    g_write_fd    = get_child_write_fd(child_index);
    if (g_write_fd == -1) {
        fprintf(stderr,
            "[child_ipc] init: get_child_write_fd(%d) returned -1\n",
            child_index);
        return -1;
    }

    /*
     * Close every pipe end we do not own.
     * For pipe i:
     *   i == child_index → close the read end only (we only write)
     *   i != child_index → close both ends (not ours at all)
     */
    for (int i = 0; i < g_total_children; i++) {
        int w_fd = get_child_write_fd(i);
        if (w_fd == -1) continue;
        int r_fd = w_fd - 1;   /* pipe() allocates [read, write] consecutively */

        if (i == child_index) {
            if (close(r_fd) == -1 && errno != EBADF)
                perror("[child_ipc] close own read end");
        } else {
            if (close(r_fd) == -1 && errno != EBADF)
                perror("[child_ipc] close sibling read end");
            if (close(w_fd) == -1 && errno != EBADF)
                perror("[child_ipc] close sibling write end");
        }
    }

    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  write_message – atomic write of one StatusMessage to the pipe
 * ════════════════════════════════════════════════════════════════════ */
static int write_message(const StatusMessage *msg)
{
    if (g_write_fd == -1) {
        fprintf(stderr, "[child_ipc] send: not initialised\n");
        return -1;
    }

    ssize_t total     = 0;
    ssize_t remaining = (ssize_t)sizeof(*msg);
    const char *buf   = (const char *)msg;

    while (remaining > 0) {
        ssize_t n = write(g_write_fd, buf + total, (size_t)remaining);
        if (n == -1) {
            if (errno == EINTR) continue;   /* retry on signal interrupt */
            perror("[child_ipc] write");
            return -1;
        }
        total     += n;
        remaining -= n;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  send_status_m6  – Milestone 6: send a full status message
 * ════════════════════════════════════════════════════════════════════ */
int send_status_m6(int current_node, int next_node,
                   bool is_destination, bool is_finished,
                   SyncState sync_state, int target_node)
{
    StatusMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.pid            = getpid();
    msg.current_node   = current_node;
    msg.next_node      = next_node;
    msg.is_destination = is_destination;
    msg.is_finished    = is_finished;
    msg.sync_state     = (int)sync_state;
    msg.target_node    = target_node;
    return write_message(&msg);
}

/* ════════════════════════════════════════════════════════════════════
 *  send_status_to_parent  – Milestone 5 API (backward compatible)
 *  Delegates to send_status_m6 with sensible defaults for new fields.
 * ════════════════════════════════════════════════════════════════════ */
int send_status_to_parent(int current_node, int next_node,
                           bool is_destination, bool is_finished)
{
    return send_status_m6(current_node, next_node,
                           is_destination, is_finished,
                           SYNC_MOVING,
                           next_node);   /* target = next_node */
}

/* ════════════════════════════════════════════════════════════════════
 *  close_child_ipc_side
 * ════════════════════════════════════════════════════════════════════ */
void close_child_ipc_side(void)
{
    if (g_write_fd != -1) {
        if (close(g_write_fd) == -1 && errno != EBADF)
            perror("[child_ipc] close write end");
        g_write_fd = -1;
    }
}