/*
 * parent_ipc.c  –  Milestone 5: Parent Receiver API Implementation
 *
 * Implements a UNIX-pipe-based MPSC (Multi-Producer Single-Consumer)
 * communication channel.  One pipe pair is allocated per child; the parent
 * owns all read ends (O_NONBLOCK) and each child owns exactly one write end.
 *
 * Why UNIX Pipes with O_NONBLOCK?  (see README section below)
 * ──────────────────────────────────────────────────────────
 * See the README excerpt at the bottom of this file.
 */

#define _POSIX_C_SOURCE 200809L

#include "parent_ipc.h"
#include "ipc_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Each entry holds a [read_fd, write_fd] pair for one child.
 *  • pipe_fds[i][0] = read end  (parent polls this; set O_NONBLOCK)
 *  • pipe_fds[i][1] = write end (child writes here; closed in parent after fork)
 */
static int  (*pipe_fds)[2] = NULL;   /* dynamically allocated 2-D array */
static int   g_num_children = 0;     /* set by init_ipc_infrastructure() */

/* ═══════════════════════════════════════════════════════════════════════════
 *  init_ipc_infrastructure
 * ═══════════════════════════════════════════════════════════════════════════ */
int init_ipc_infrastructure(int num_children)
{
    if (num_children <= 0) {
        fprintf(stderr, "[parent_ipc] init: num_children must be > 0\n");
        return -1;
    }

    /* Allocate the pipe-fd table */
    pipe_fds = malloc((size_t)num_children * sizeof(*pipe_fds));
    if (!pipe_fds) {
        perror("[parent_ipc] malloc pipe_fds");
        return -1;
    }

    /* Initialise every entry to -1 so cleanup_ipc_infrastructure() can
     * safely skip descriptors that were never opened.                   */
    for (int i = 0; i < num_children; i++) {
        pipe_fds[i][0] = -1;
        pipe_fds[i][1] = -1;
    }

    g_num_children = num_children;

    /* Create one pipe per child and mark the read end non-blocking */
    for (int i = 0; i < num_children; i++) {

        if (pipe(pipe_fds[i]) == -1) {
            perror("[parent_ipc] pipe");
            /* Tear down everything opened so far before returning */
            cleanup_ipc_infrastructure();
            return -1;
        }

        /* Set O_NONBLOCK on the READ end so the GUI loop never stalls */
        int flags = fcntl(pipe_fds[i][0], F_GETFL, 0);
        if (flags == -1) {
            perror("[parent_ipc] fcntl F_GETFL");
            cleanup_ipc_infrastructure();
            return -1;
        }
        if (fcntl(pipe_fds[i][0], F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("[parent_ipc] fcntl F_SETFL O_NONBLOCK");
            cleanup_ipc_infrastructure();
            return -1;
        }
    }

    return 0;   /* success */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  close_parent_write_ends
 * ═══════════════════════════════════════════════════════════════════════════ */
void close_parent_write_ends(void)
{
    if (!pipe_fds) return;

    for (int i = 0; i < g_num_children; i++) {
        if (pipe_fds[i][1] != -1) {
            if (close(pipe_fds[i][1]) == -1) {
                perror("[parent_ipc] close write end");
            }
            pipe_fds[i][1] = -1;   /* mark closed to prevent double-close */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  read_status_from_child
 * ═══════════════════════════════════════════════════════════════════════════ */
int read_status_from_child(int child_index, StatusMessage *out_msg)
{
    /* Validate arguments */
    if (!pipe_fds || child_index < 0 || child_index >= g_num_children) {
        fprintf(stderr,
            "[parent_ipc] read_status_from_child: invalid index %d\n",
            child_index);
        return -1;
    }

    int fd = pipe_fds[child_index][0];
    if (fd == -1) {
        /* This pipe was already closed (child finished) */
        return -1;
    }

    /* Attempt a non-blocking read of exactly one StatusMessage.
     * Because O_NONBLOCK is set, read() returns immediately with:
     *   n == sizeof(StatusMessage)  → full message received
     *   n == -1, errno == EAGAIN    → no data yet (try next frame)
     *   n == 0                      → writer closed the pipe (EOF)
     *   n > 0, n < sizeof(...)      → partial read (should never happen
     *                                 for payloads < PIPE_BUF; treated as
     *                                 error to keep the state machine clean)
     */
    ssize_t n = read(fd, out_msg, sizeof(StatusMessage));

    if (n == (ssize_t)sizeof(StatusMessage)) {
        return 1;   /* success – message ready */
    }

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   /* no data available right now; not an error */
        }
        /* Real I/O error */
        perror("[parent_ipc] read");
        return -1;
    }

    if (n == 0) {
        /* EOF: the child closed its write end (has exited or called
         * close() on the fd).  Close our read end as well.          */
        if (close(pipe_fds[child_index][0]) == -1) {
            perror("[parent_ipc] close read end (EOF)");
        }
        pipe_fds[child_index][0] = -1;
        return -1;
    }

    /* Partial read – highly unlikely for a struct < PIPE_BUF, but
     * handle it defensively.                                         */
    fprintf(stderr,
        "[parent_ipc] read_status_from_child: partial read (%zd/%zu bytes) "
        "on child %d – channel may be corrupted\n",
        n, sizeof(StatusMessage), child_index);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  cleanup_ipc_infrastructure
 * ═══════════════════════════════════════════════════════════════════════════ */
void cleanup_ipc_infrastructure(void)
{
    if (!pipe_fds) return;

    for (int i = 0; i < g_num_children; i++) {
        /* Close read end */
        if (pipe_fds[i][0] != -1) {
            if (close(pipe_fds[i][0]) == -1 && errno != EBADF) {
                perror("[parent_ipc] cleanup: close read end");
            }
            pipe_fds[i][0] = -1;
        }
        /* Close write end (only still open if close_parent_write_ends
         * was never called – e.g. abnormal exit path)               */
        if (pipe_fds[i][1] != -1) {
            if (close(pipe_fds[i][1]) == -1 && errno != EBADF) {
                perror("[parent_ipc] cleanup: close write end");
            }
            pipe_fds[i][1] = -1;
        }
    }

    free(pipe_fds);
    pipe_fds       = NULL;
    g_num_children = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  get_child_write_fd
 * ═══════════════════════════════════════════════════════════════════════════ */
int get_child_write_fd(int child_index)
{
    if (!pipe_fds || child_index < 0 || child_index >= g_num_children) {
        return -1;
    }
    return pipe_fds[child_index][1];
}