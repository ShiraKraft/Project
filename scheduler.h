/*
 * scheduler.h  –  Milestone 7: Scheduling Algorithm (FCFS / SJF)
 *
 * Builds on top of the existing WaitingQueue infrastructure.
 * QueueNode is a lightweight linked-list wrapper used by the scheduler.
 * SchedulerState manages the head pointer and active algorithm type.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include "waiting_queue.h"   /* QueuedTraveler, WaitingQueue */

/* ── Algorithm type ─────────────────────────────────────────────────────── */
typedef enum {
    SCHED_FCFS = 0,   /* First-Come, First-Served  */
    SCHED_SJF  = 1    /* Shortest-Job-First        */
} SchedType;

/* ── QueueNode ──────────────────────────────────────────────────────────── */
typedef struct QueueNode {
    int             child_id;       /* traveler / child-process ID           */
    int             estimated_time; /* burst time used for SJF ordering      */
    struct QueueNode *next;
} QueueNode;

/* ── SchedulerState ─────────────────────────────────────────────────────── */
typedef struct {
    QueueNode *head;       /* front of the linked list                       */
    SchedType  type;       /* active scheduling algorithm                    */
    bool       is_empty;   /* convenience flag; true when head == NULL       */
} SchedulerState;

/* ── Function declarations ──────────────────────────────────────────────── */
void init_scheduler  (SchedType type);
void schedule_add    (int child_id, int estimated_time);
int  schedule_next   (void);
void test_algorithm_a(void);
void generate_test_scenario(const char *filename);
void destroy_scheduler(void);

#endif /* SCHEDULER_H */
