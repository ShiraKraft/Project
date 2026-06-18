#ifndef WAITING_QUEUE_H
#define WAITING_QUEUE_H

#include <sys/types.h>
#include <stdbool.h>

/* Structure representing a traveler currently waiting in a node's queue */
typedef struct {
    pid_t pid;          /* PID of the child process (traveler) */
    int priority;       /* Traveler priority */
    int arrival_time;   /* Timestamp when the traveler arrived at the current node */
    int burst_time;     /* Requested service/burst time (used for SJF scheduling) */
} QueuedTraveler;

/* Structure representing the waiting queue of a single graph node */
typedef struct {
    QueuedTraveler travelers[32]; /* Maximum of 32 waiting travelers (MAX_TRAVELERS) */
    int size;                     /* Current number of travelers in the queue */
} WaitingQueue;

/* Queue management function declarations */
void init_queue(WaitingQueue *q);
void enqueue_fcfs(WaitingQueue *q, pid_t pid, int priority, int arrival_time, int burst_time);
void enqueue_sjf(WaitingQueue *q, pid_t pid, int priority, int arrival_time, int burst_time);
pid_t dequeue_front(WaitingQueue *q);
int get_traveler_queue_index(WaitingQueue *q, pid_t pid);

#endif /* WAITING_QUEUE_H */
