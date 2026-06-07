#ifndef CHILD_H
#define CHILD_H

/*
 * run_child – entry point for the child process after fork().
 *
 * Must be called only in the child branch:
 *
 *   pid_t pid = fork();
 *   if (pid == 0) {
 *       run_child(path, path_len, weights);
 *       // never returns
 *   }
 *
 * Parameters:
 *   path      – ordered list of node indices (source … destination)
 *   path_len  – total number of nodes in path  (>= 1)
 *   weights   – edge weights; weights[i] is the weight of the edge
 *               from path[i] to path[i+1]; array length = path_len-1
 *
 * The function never returns; it calls exit() internally.
 */
void run_child(const int *path, int path_len, const int *weights);
/*
 * child.h – Milestone 5: Autonomous Child Process
 *
 * In milestone 5, the child receives only the graph and its src/dst.
 * It computes Dijkstra independently and sends status updates to the
 * parent via pipe (using child_ipc / send_status_to_parent).
 *
 * The parent must NOT pass the computed path to the child.
 */
 
#include "graph.h"
 
/*
 * run_child_m5 – entry point for the milestone-5 child process.
 *
 * Called only in the child branch (after fork()):
 *
 *   pid_t pid = fork();
 *   if (pid == 0) {
 *       run_child_m5(graph, src, dst, child_index, total_children);
 *       // never returns
 *   }
 *
 * Parameters:
 *   graph          – pointer to the loaded graph (inherited from parent)
 *   src            – source node index for this traveler
 *   dst            – destination node index for this traveler
 *   child_index    – index of this child in the pipe table (0-based)
 *   total_children – total number of children (needed to close other pipes)
 *
 * The function:
 *   1. Initialises the IPC write-side (closes unneeded pipe fds)
 *   2. Runs Dijkstra locally to find src→dst path
 *   3. Sends a StatusMessage for every node in the path (via pipe)
 *   4. Sends a final "finished" message
 *   5. Calls exit(EXIT_SUCCESS)
 *
 * The function never returns.
 */
void run_child_m5(int src, int dst, const char *filename, int child_index, int total_children);
 
#endif /* CHILD_H */
