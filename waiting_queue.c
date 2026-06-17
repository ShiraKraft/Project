#include <stdio.h>
#include <string.h>
#include "waiting_queue.h"

/* Initialize a node's waiting queue */
void init_queue(WaitingQueue *q) {
    if (q != NULL) {
        q->size = 0;
        memset(q->travelers, 0, sizeof(q->travelers));
    }
}

/* Insert a traveler into the queue using First-Come, First-Served (FCFS) */
void enqueue_fcfs(WaitingQueue *q, pid_t pid, int priority, int arrival_time, int burst_time) {
    if (q == NULL || q->size >= 32) return;

    /* FCFS simply appends to the end of the queue */
    q->travelers[q->size].pid = pid;
    q->travelers[q->size].priority = priority;
    q->travelers[q->size].arrival_time = arrival_time;
    q->travelers[q->size].burst_time = burst_time;
    q->size++;
}

/* Insert a traveler into the queue using Shortest Job First (SJF) */
void enqueue_sjf(WaitingQueue *q, pid_t pid, int priority, int arrival_time, int burst_time) {
    if (q == NULL || q->size >= 32) return;

    /* Find the correct insertion index to maintain sorted order by burst_time */
    int i = q->size - 1;
    while (i >= 0 && q->travelers[i].burst_time > burst_time) {
        /* Shift element to the right */
        q->travelers[i + 1] = q->travelers[i];
        i--;
    }

    /* Place the new traveler in its sorted position */
    q->travelers[i + 1].pid = pid;
    q->travelers[i + 1].priority = priority;
    q->travelers[i + 1].arrival_time = arrival_time;
    q->travelers[i + 1].burst_time = burst_time;
    q->size++;
}

/* Remove and return the front traveler from the queue */
pid_t dequeue_front(WaitingQueue *q) {
    if (q == NULL || q->size <= 0) return -1;

    pid_t front_pid = q->travelers[0].pid;

    /* Shift all remaining elements left */
    for (int i = 1; i < q->size; i++) {
        q->travelers[i - 1] = q->travelers[i];
    }
    q->size--;

    return front_pid;
}

/* Find the index of a specific traveler by PID, returns -1 if not found */
int get_traveler_queue_index(WaitingQueue *q, pid_t pid) {
    if (q == NULL) return -1;

    for (int i = 0; i < q->size; i++) {
        if (q->travelers[i].pid == pid) {
            return i;
        }
    }
    return -1;
}