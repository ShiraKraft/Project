/*
 * scheduler.c  –  Milestone 7: Scheduling Algorithm (FCFS / SJF)
 *
 * FIX (M7): destroy_scheduler() was erroneously nested as a local function
 * definition inside generate_test_scenario().  It is now a proper top-level
 * function declared in scheduler.h.  All other logic is unchanged.
 *
 * Uses the existing WaitingQueue from waiting_queue.c for the actual
 * per-node queues; the SchedulerState linked list here is the global
 * intersection scheduler that decides which traveler enters next.
 */

#include "scheduler.h"
#include "waiting_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* ── Global scheduler instance (one per program) ───────────────────────── */
static SchedulerState g_scheduler = { NULL, SCHED_FCFS, true };

/* ═══════════════════════════════════════════════════════════════════════════
 *  A.  init_scheduler
 *      Resets the linked list and records the chosen algorithm.
 * ═══════════════════════════════════════════════════════════════════════════ */
void init_scheduler(SchedType type)
{
    /* Free any leftover nodes from a previous run */
    QueueNode *cur = g_scheduler.head;
    while (cur) {
        QueueNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }

    g_scheduler.head     = NULL;
    g_scheduler.type     = type;
    g_scheduler.is_empty = true;

    printf("[Scheduler] Initialised with algorithm: %s\n",
           type == SCHED_SJF ? "SJF" : "FCFS");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  B.  schedule_add
 *      FCFS → append to tail (arrival order preserved).
 *      SJF  → ordered insert ascending on estimated_time; ties keep
 *             arrival order (stable sort property).
 * ═══════════════════════════════════════════════════════════════════════════ */
void schedule_add(int child_id, int estimated_time)
{
    QueueNode *node = (QueueNode *)malloc(sizeof(QueueNode));
    if (!node) {
        fprintf(stderr, "[Scheduler] malloc failed in schedule_add\n");
        return;
    }
    node->child_id       = child_id;
    node->estimated_time = estimated_time;
    node->next           = NULL;

    /* ── FCFS: append to tail ─────────────────────────────────────────── */
    if (g_scheduler.type == SCHED_FCFS) {
        if (!g_scheduler.head) {
            g_scheduler.head = node;
        } else {
            QueueNode *cur = g_scheduler.head;
            while (cur->next) cur = cur->next;
            cur->next = node;
        }

    /* ── SJF: ordered insert (ascending estimated_time) ──────────────── */
    } else {
        if (!g_scheduler.head ||
            node->estimated_time < g_scheduler.head->estimated_time) {
            /* Insert at front */
            node->next           = g_scheduler.head;
            g_scheduler.head     = node;
        } else {
            QueueNode *cur = g_scheduler.head;
            while (cur->next &&
                   cur->next->estimated_time <= node->estimated_time) {
                cur = cur->next;
            }
            node->next = cur->next;
            cur->next  = node;
        }
    }

    g_scheduler.is_empty = false;
    printf("[Scheduler] Added child_id=%d burst=%d  (algo=%s)\n",
           child_id, estimated_time,
           g_scheduler.type == SCHED_SJF ? "SJF" : "FCFS");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  C.  schedule_next
 *      Pops and returns the child_id at the head of the queue.
 *      Returns -1 if the queue is empty.
 * ═══════════════════════════════════════════════════════════════════════════ */
int schedule_next(void)
{
    if (!g_scheduler.head) return -1;

    QueueNode *front  = g_scheduler.head;
    int        result = front->child_id;

    g_scheduler.head = front->next;
    free(front);

    g_scheduler.is_empty = (g_scheduler.head == NULL);

    printf("[Scheduler] Dispatching child_id=%d\n", result);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  D.  destroy_scheduler
 *      Drains the queue and frees all nodes.  Safe to call multiple times.
 *
 *  BUG FIX: This was previously a nested function inside
 *  generate_test_scenario(), which is undefined behaviour in C11.
 *  It is now a proper file-scope function matching the header declaration.
 * ═══════════════════════════════════════════════════════════════════════════ */
void destroy_scheduler(void)
{
    /* Drain remaining nodes */
    while (g_scheduler.head) {
        QueueNode *tmp   = g_scheduler.head;
        g_scheduler.head = tmp->next;
        free(tmp);
    }
    g_scheduler.is_empty = true;
    printf("[Scheduler] Resources destroyed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  E.  test_algorithm_a  –  Unit tests for FCFS and SJF
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_algorithm_a(void)
{
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║   Milestone 7 – Scheduler Unit Tests    ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* ── Test 1: FCFS – arrivals served in arrival order ─────────────── */
    printf("\n[TEST 1] FCFS – arrival order preserved\n");
    init_scheduler(SCHED_FCFS);
    schedule_add(10, 5);
    schedule_add(20, 2);
    schedule_add(30, 8);
    schedule_add(40, 1);

    assert(schedule_next() == 10 && "FCFS: first in must be first out");
    assert(schedule_next() == 20 && "FCFS: second in must be second out");
    assert(schedule_next() == 30 && "FCFS: third in must be third out");
    assert(schedule_next() == 40 && "FCFS: fourth in must be fourth out");
    assert(schedule_next() == -1 && "FCFS: empty queue must return -1");
    printf("[PASS] FCFS order correct\n");

    /* ── Test 2: FCFS – single element ───────────────────────────────── */
    printf("\n[TEST 2] FCFS – single element\n");
    init_scheduler(SCHED_FCFS);
    schedule_add(99, 3);
    assert(schedule_next() == 99 && "FCFS single element");
    assert(schedule_next() == -1 && "FCFS empty after single pop");
    printf("[PASS] FCFS single element correct\n");

    /* ── Test 3: SJF – sorted by estimated_time ascending ────────────── */
    printf("\n[TEST 3] SJF – shortest job served first\n");
    init_scheduler(SCHED_SJF);
    schedule_add(10, 8);   /* long  job arrives first  */
    schedule_add(20, 2);   /* short job arrives second */
    schedule_add(30, 5);   /* mid   job arrives third  */

    assert(schedule_next() == 20 && "SJF: shortest (t=2) must go first");
    assert(schedule_next() == 30 && "SJF: middle  (t=5) must go second");
    assert(schedule_next() == 10 && "SJF: longest (t=8) must go last");
    assert(schedule_next() == -1 && "SJF: empty queue must return -1");
    printf("[PASS] SJF order correct\n");

    /* ── Test 4: SJF – tie-breaking (equal burst time) ───────────────── */
    printf("\n[TEST 4] SJF – equal burst times keep stable order\n");
    init_scheduler(SCHED_SJF);
    schedule_add(1, 4);
    schedule_add(2, 4);
    schedule_add(3, 4);
    assert(schedule_next() == 1 && "SJF tie: first arrival first");
    assert(schedule_next() == 2 && "SJF tie: second arrival second");
    assert(schedule_next() == 3 && "SJF tie: third arrival third");
    printf("[PASS] SJF tie-breaking correct\n");

    /* ── Test 5: SJF – short job inserted after long job ─────────────── */
    printf("\n[TEST 5] SJF – short job arriving after long job preempts\n");
    init_scheduler(SCHED_SJF);
    schedule_add(100, 10);  /* long job first */
    schedule_add(200,  1);  /* short job second – must jump to front */
    assert(schedule_next() == 200 && "SJF: late short job must go before early long job");
    assert(schedule_next() == 100 && "SJF: long job must follow");
    printf("[PASS] SJF late short job handled correctly\n");

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      All 5 scheduler tests passed ✓     ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    destroy_scheduler();   /* clean up after tests */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  F.  generate_test_scenario
 *
 *  Writes a travelers input file that forces SJF to demonstrate its
 *  advantage over FCFS.  File format (matches ParseExtendedInputFiles):
 *    Line 1: number_of_travelers
 *    Lines 2+: src dst burst_time
 *
 *  Expected SJF  order: traveler 3 → 1 → 2 → 0
 *  Expected FCFS order: traveler 0 → 1 → 2 → 3
 * ═══════════════════════════════════════════════════════════════════════════ */
void generate_test_scenario(const char *filename)
{
    if (!filename) return;

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "generate_test_scenario: cannot open '%s'\n", filename);
        return;
    }

    fprintf(fp, "4\n");          /* total_travelers */
    fprintf(fp, "0 4 9\n");      /* traveler 0: long  job, arrives first  */
    fprintf(fp, "1 3 2\n");      /* traveler 1: short job, arrives second */
    fprintf(fp, "0 2 5\n");      /* traveler 2: mid   job, arrives third  */
    fprintf(fp, "2 4 1\n");      /* traveler 3: very short, arrives last  */

    fclose(fp);
    printf("[M7] Test scenario written to '%s'\n", filename);
    fflush(stdout);
}