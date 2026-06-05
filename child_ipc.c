/*
 * child_ipc.c  –  Milestone 5: Child Sender API Implementation
 *
 * Each child process calls init_child_ipc_side() once after fork(), then
 * calls send_status_to_parent() for every graph node it arrives at during
 * its autonomous Dijkstra traversal.
 *
 * File-descriptor hygiene
 * ───────────────────────
 * After fork(), the child inherits ALL pipe descriptors created by the
 * parent.  We must close every fd that belongs to other children (both
 * ends) and close the read end of our own pipe.  Failure to do so means:
 *   • Stray write ends in the child prevent the parent's read() from ever
 *     returning EOF on the sibling pipes → parent hangs on cleanup.
 *   • Stray read ends in the child are benign but waste file descriptors.
 *
 * We use get_child_write_fd() + an internal mirror of the fd table that
 * was initialised in parent_ipc.c before fork() to enumerate all fds.
 */

#define _POSIX_C_SOURCE 200809L

#include "child_ipc.h"
#include "parent_ipc.h"     /* get_child_write_fd()        */
#include "ipc_protocol.h"   /* StatusMessage               */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Module-level state  (per-child-process only; not shared with parent)
 * ═══════════════════════════════════════════════════════════════════════════ */

static int  g_write_fd    = -1;   /* our write end after init */
static int  g_child_index = -1;   /* our own index            */
static int  g_total_children = 0; /* total pipes to close     */

/*
 * child_ipc_set_total_children
 *
 * Called by milestone5 main entry to inform child_ipc how many pipes exist
 * in total so it can close all the ones it does not own.
 *
 * Must be called BEFORE fork() (in the parent, so the value is inherited).
 */
void child_ipc_set_total_children(int n)
{
    g_total_children = n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  init_child_ipc_side
 * ═══════════════════════════════════════════════════════════════════════════ */
int init_child_ipc_side(int child_index)
{
    if (child_index < 0 || g_total_children <= 0) {
        fprintf(stderr,
            "[child_ipc] init: invalid child_index=%d total=%d\n",
            child_index, g_total_children);
        return -1;
    }

    g_child_index = child_index;

    /* Retrieve our write end BEFORE closing anything */
    g_write_fd = get_child_write_fd(child_index);
    if (g_write_fd == -1) {
        fprintf(stderr,
            "[child_ipc] init: get_child_write_fd(%d) returned -1\n",
            child_index);
        return -1;
    }

    /*
     * Close every pipe end we do not own.
     *
     * For each pipe i:
     *   • If i == child_index  → close the READ end (we only write)
     *   • If i != child_index  → close BOTH ends (not ours at all)
     *
     * get_child_write_fd(i) is the write fd for pipe i.
     * The read fd for pipe i is write_fd - 1 because pipe() always
     * allocates consecutive fds: [read, write].  We cannot call
     * get_child_read_fd() here (no such exported symbol), but the
     * consecutive-allocation invariant holds on Linux and all POSIX
     * targets we support.  We use (write_fd - 1) as the read fd.
     */
    for (int i = 0; i < g_total_children; i++) {
        int w_fd = get_child_write_fd(i);
        if (w_fd == -1) continue;       /* already closed in parent */

        int r_fd = w_fd - 1;            /* pipe() allocates [r, w] consecutively */

        if (i == child_index) {
            /* Close only the read end of our own pipe */
            if (close(r_fd) == -1 && errno != EBADF) {
                perror("[child_ipc] close own read end");
                /* Non-fatal – continue */
            }
        } else {
            /* Close both ends of every other child's pipe */
            if (close(r_fd) == -1 && errno != EBADF) {
                perror("[child_ipc] close sibling read end");
            }
            if (close(w_fd) == -1 && errno != EBADF) {
                perror("[child_ipc] close sibling write end");
            }
        }
    }

    return 0;   /* success */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  send_status_to_parent
 * ═══════════════════════════════════════════════════════════════════════════ */
int send_status_to_parent(int current_node, int next_node,
                           bool is_destination, bool is_finished)
{
    if (g_write_fd == -1) {
        fprintf(stderr,
            "[child_ipc] send_status_to_parent: not initialised "
            "(call init_child_ipc_side first)\n");
        return -1;
    }

    /* Assemble the message */
    StatusMessage msg;
    memset(&msg, 0, sizeof(msg));       /* zero-pad for safety / valgrind */
    msg.pid            = getpid();
    msg.current_node   = current_node;
    msg.next_node      = next_node;
    msg.is_destination = is_destination;
    msg.is_finished    = is_finished;

    /*
     * write() is atomic for payloads ≤ PIPE_BUF (≥ 512 B; Linux: 4096 B).
     * sizeof(StatusMessage) << PIPE_BUF, so the kernel guarantees the
     * entire struct lands in one write without interleaving with sibling
     * children's writes.  We still loop on EINTR to handle signal delivery.
     */
    ssize_t total = 0;
    const char *buf = (const char *)&msg;
    ssize_t remaining = (ssize_t)sizeof(msg);

    while (remaining > 0) {
        ssize_t n = write(g_write_fd, buf + total, (size_t)remaining);
        if (n == -1) {
            if (errno == EINTR) continue;   /* interrupted by signal – retry */
            perror("[child_ipc] write");
            return -1;
        }
        total     += n;
        remaining -= n;
    }

    return 0;   /* success */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  close_child_ipc_side
 * ═══════════════════════════════════════════════════════════════════════════ */
void close_child_ipc_side(void)
{
    if (g_write_fd != -1) {
        if (close(g_write_fd) == -1 && errno != EBADF) {
            perror("[child_ipc] close write end");
        }
        g_write_fd = -1;
    }
}