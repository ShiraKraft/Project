#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h> 

/* ------------------------------------------------------------------ */
/* IPC Message Structure Definition                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    pid_t child_pid;
    int current_node;
    int next_node;
    int is_destination; /* 1 if arrived at destination, 0 otherwise */
    int is_finished;    /* 1 if process is completely done, 0 otherwise */
} IPC_Message;

/* ------------------------------------------------------------------ */
/* run_child - Milestone 5 Version (Autonomous & IPC-based)           */
/* ------------------------------------------------------------------ */
void run_child(const int *path, int path_len, const int *weights)
{
    /* In milestone 5, children write to the IPC channel.
     * We assume write_fd = 3 is the dedicated pipe descriptor passed 
     * by the parent, or managed via dup2/stdout redirect.
     */
    int write_fd = 3; 

    /* 1. Autonomous Travel Simulation */
    for (int i = 0; i < path_len; i++) {
        IPC_Message msg;
        msg.child_pid = getpid();
        msg.current_node = path[i];
        msg.is_finished = 0;

        /* Check if we have reached the final destination node */
        if (i == path_len - 1) {
            msg.next_node = -1;
            msg.is_destination = 1;
        } else {
            msg.next_node = path[i + 1];
            msg.is_destination = 0;
        }

        // ==========================================================
        // SYNCHRONIZATION: Access and lock the unique node semaphore
        // ==========================================================
        char sem_name[32];
        sprintf(sem_name, "/node_sem_%d", msg.current_node);
        
        /* Open or connect to the named semaphore for the current node (initially 1) */
        sem_t *node_sem = sem_open(sem_name, O_CREAT, 0666, 1);
        
        /* Block and wait here if another passenger process currently occupies this node */
        sem_wait(node_sem); 
        // ==========================================================

        /* Send current location update to parent via IPC pipe */
        /* Only executed AFTER successfully acquiring the node lock */
        write(write_fd, &msg, sizeof(IPC_Message));

        /* If destination reached, terminate the travel loop */
        if (msg.is_destination) {
            /* Release the node semaphore before exiting the loop */
            sem_post(node_sem);
            sem_close(node_sem);
            break;
        }

        /* Travel across the edge: W hops × 300 ms each */
        int W = weights[i];
        for (int hop = 0; hop < W; hop++) {
            usleep(300 * 1000); /* 300 ms */
        }

        /* Intermediate node wait time (1 second) */
        usleep(1000 * 1000); /* 1s */

        // ==========================================================
        // SYNCHRONIZATION: Departure from node - release the lock
        // ==========================================================
        /* Unlock the semaphore to allow the next waiting passenger to enter */
        sem_post(node_sem);  
        /* Close the local descriptor link to this named semaphore */
        sem_close(node_sem); 
        // ==========================================================
    }

    /* 2. Send final completion message to parent */
    IPC_Message end_msg;
    memset(&end_msg, 0, sizeof(IPC_Message));
    end_msg.child_pid = getpid();
    end_msg.is_finished = 1;
    write(write_fd, &end_msg, sizeof(IPC_Message));

    /* 3. Resource cleanup and clean exit */
    close(write_fd);
    exit(EXIT_SUCCESS);
}