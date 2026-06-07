/*
 * milestone5.h – Milestone 5: Autonomous Child Process Interface
 *
 * Declares the single entry-point called in the child branch after fork().
 * The child reads the graph file itself, runs Dijkstra independently,
 * and reports every node arrival to the parent via pipe IPC.
 *
 * The parent MUST NOT pass path data to run_child_m5() – data separation
 * is a hard requirement of milestone 5.
 */

#ifndef MILESTONE5_H
#define MILESTONE5_H

/*
 * run_child_m5 – called in the child process (after fork() returns 0).
 *
 * Parameters:
 *   filename    – path to the shared input file (graph + travelers section)
 *   src         – this traveler's source node
 *   dst         – this traveler's destination node
 *   child_index – 0-based index of this child (matches pipe index)
 *   total       – total number of children (used to close other pipes;
 *                 already set via child_ipc_set_total_children() in parent)
 *
 * This function never returns – it always ends with exit().
 */
void run_child_m5(int src, int dst, const char *filename, int child_index, int total_children);

#endif /* MILESTONE5_H */