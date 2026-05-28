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

#endif /* CHILD_H */
