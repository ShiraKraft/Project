/*
 * tester.c  -  OS Project Test Suite
 *              Milestone 1: Dijkstra correctness tests (T1-T12)
 *              Milestone 6: Lock-integrity validation hooks
 *
 * Compile (Milestone 1 tests only):
 *   gcc -o tester tester.c graph.c -lm
 *   ./tester ./dijkstra
 *
 * Compile (with Milestone 6 lock hooks enabled):
 *   gcc -o tester tester.c graph.c -lm -lrt
 *
 * Or simply:
 *   make test
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "graph.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Colour helpers
 * ═══════════════════════════════════════════════════════════════════ */
#define GREEN  "\033[0;32m"
#define RED    "\033[0;31m"
#define YELLOW "\033[1;33m"
#define CYAN   "\033[0;36m"
#define RESET  "\033[0m"

#include <sys/wait.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Global counters
 * ═══════════════════════════════════════════════════════════════════ */
static int g_pass  = 0;
static int g_fail  = 0;
static int g_total = 0;

/* ═══════════════════════════════════════════════════════════════════
 *  write_tmp_file  -  dump a C-string to a temp file, return path
 * ═══════════════════════════════════════════════════════════════════ */
static const char *write_tmp_file(const char *name, const char *content)
{
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/os_test_%s.txt", name);
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen tmp"); exit(EXIT_FAILURE); }
    fputs(content, f);
    fclose(f);
    return path;
}

/* ═══════════════════════════════════════════════════════════════════
 *  capture_stdout  -  run "./Project <file>", return stdout as string
 *  (caller must free the returned buffer)
 * ═══════════════════════════════════════════════════════════════════ */
static char *capture_stdout(const char *exe, const char *input_file)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" 2>/dev/null", exe, input_file);

    FILE *fp = popen(cmd, "r");
    if (!fp) { perror("popen"); exit(EXIT_FAILURE); }

    size_t cap = 1024, len = 0;
    char *buf = malloc(cap);
    if (!buf) { perror("malloc"); exit(EXIT_FAILURE); }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    pclose(fp);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════
 *  trim_trailing  -  remove trailing whitespace / newlines in-place
 * ═══════════════════════════════════════════════════════════════════ */
static void trim_trailing(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' '))
        s[--n] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 *  check  -  compare actual vs expected, print result
 * ═══════════════════════════════════════════════════════════════════ */
static void check(const char *test_name,
                  const char *actual,
                  const char *expected)
{
    g_total++;
    char a[2048], e[2048];
    strncpy(a, actual,   sizeof(a)-1); a[sizeof(a)-1]='\0';
    strncpy(e, expected, sizeof(e)-1); e[sizeof(e)-1]='\0';
    trim_trailing(a);
    trim_trailing(e);

    if (strcmp(a, e) == 0) {
        printf(GREEN "  [PASS]" RESET " %s\n", test_name);
        g_pass++;
    } else {
        printf(RED   "  [FAIL]" RESET " %s\n", test_name);
        printf("         Expected : %s\n", e);
        printf("         Got      : %s\n", a);
        g_fail++;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  check_error  -  verify program exits with non-zero code
 * ═══════════════════════════════════════════════════════════════════ */
static void check_error(const char *test_name,
                         const char *exe,
                         const char *input_file)
{
    g_total++;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" >/dev/null 2>&1", exe, input_file);
    int ret = system(cmd);
    int exit_code = WEXITSTATUS(ret);

    if (exit_code != 0) {
        printf(GREEN "  [PASS]" RESET " %s  (exit=%d)\n", test_name, exit_code);
        g_pass++;
    } else {
        printf(RED   "  [FAIL]" RESET " %s  - expected non-zero exit, got 0\n",
               test_name);
        g_fail++;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  MILESTONE 1 TESTS  (T1 - T12)
 * ═══════════════════════════════════════════════════════════════════ */

static void test_assignment_example(const char *exe)
{
    const char *graph =
        "6 8\n"
        "0 1 4\n"
        "0 2 2\n"
        "1 3 5\n"
        "2 1 1\n"
        "2 3 8\n"
        "3 4 2\n"
        "4 5 3\n"
        "2 5 10\n"
        "0 5\n";

    const char *path = write_tmp_file("t1", graph);
    char *out = capture_stdout(exe, path);
    trim_trailing(out);

    if (strstr(out, "12") != NULL && strstr(out, "->") != NULL) {
        g_total++;
        printf(GREEN "  [PASS]" RESET " T1 - Assignment example (weight=12, path printed)\n");
        g_pass++;
    } else {
        g_total++;
        printf(RED "  [FAIL]" RESET " T1 - Assignment example\n");
        printf("         Expected weight 12 and a path with '->'\n");
        printf("         Got: %s\n", out);
        g_fail++;
    }
    free(out);
}

static void test_no_path(const char *exe)
{
    const char *graph =
        "4 2\n"
        "0 1 3\n"
        "2 3 5\n"
        "0 3\n";
    const char *path = write_tmp_file("t2", graph);
    char *out = capture_stdout(exe, path);
    check("T2 - Disconnected graph -> No path found", out, "No path found");
    free(out);
}

static void test_src_equals_dst(const char *exe)
{
    const char *graph =
        "3 3\n"
        "0 1 1\n"
        "1 2 2\n"
        "0 2 5\n"
        "0 0\n";
    const char *path = write_tmp_file("t3", graph);
    char *out = capture_stdout(exe, path);
    check("T3 - src == dst -> print node then 0", out, "0\n0");
    free(out);
}

static void test_negative_weight(const char *exe)
{
    const char *graph =
        "3 2\n"
        "0 1 -3\n"
        "1 2 2\n"
        "0 2\n";
    const char *path = write_tmp_file("t4", graph);
    check_error("T4 - Negative weight -> error exit", exe, path);
}

static void test_single_node(const char *exe)
{
    const char *graph =
        "1 0\n"
        "0 0\n";
    const char *path = write_tmp_file("t5", graph);
    char *out = capture_stdout(exe, path);
    check("T5 - Single node, src==dst=0", out, "0\n0");
    free(out);
}

static void test_weight_vs_hops(const char *exe)
{
    const char *graph =
        "3 3\n"
        "0 1 10\n"
        "0 2 1\n"
        "2 1 1\n"
        "0 1\n";
    const char *path = write_tmp_file("t6", graph);
    char *out = capture_stdout(exe, path);
    check("T6 - Shortest weight beats fewest hops (0->2->1, w=2)", out,
          "0 -> 2 -> 1\n2");
    free(out);
}

static void test_long_detour(const char *exe)
{
    const char *graph =
        "5 5\n"
        "0 1 100\n"
        "0 2 1\n"
        "2 3 1\n"
        "3 4 1\n"
        "4 1 1\n"
        "0 1\n";
    const char *path = write_tmp_file("t7", graph);
    char *out = capture_stdout(exe, path);
    check("T7 - Long detour cheaper than direct (0->2->3->4->1, w=4)", out,
          "0 -> 2 -> 3 -> 4 -> 1\n4");
    free(out);
}

static void test_two_nodes(const char *exe)
{
    const char *graph =
        "2 1\n"
        "0 1 7\n"
        "0 1\n";
    const char *path = write_tmp_file("t8", graph);
    char *out = capture_stdout(exe, path);
    check("T8 - Two-node graph, direct edge (w=7)", out, "0 -> 1\n7");
    free(out);
}

static void test_directed_no_return(const char *exe)
{
    const char *graph =
        "2 1\n"
        "1 0 5\n"
        "0 1\n";
    const char *path = write_tmp_file("t9", graph);
    char *out = capture_stdout(exe, path);
    check("T9 - Directed edge 1->0 only, query 0->1 -> No path found", out,
          "No path found");
    free(out);
}

static void test_missing_file(const char *exe)
{
    g_total++;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "%s /tmp/this_file_absolutely_does_not_exist_xyz123.txt"
             " >/dev/null 2>&1", exe);
    int ret = system(cmd);
    int exit_code = WEXITSTATUS(ret);
    if (exit_code != 0) {
        printf(GREEN "  [PASS]" RESET
               " T10 - Missing file -> error exit (exit=%d)\n", exit_code);
        g_pass++;
    } else {
        printf(RED "  [FAIL]" RESET
               " T10 - Missing file -> expected non-zero exit, got 0\n");
        g_fail++;
    }
}

static void test_no_args(const char *exe)
{
    g_total++;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1", exe);
    int ret = system(cmd);
    int exit_code = WEXITSTATUS(ret);
    if (exit_code != 0) {
        printf(GREEN "  [PASS]" RESET
               " T11 - No arguments -> error exit (exit=%d)\n", exit_code);
        g_pass++;
    } else {
        printf(RED "  [FAIL]" RESET
               " T11 - No arguments -> expected non-zero exit, got 0\n");
        g_fail++;
    }
}

static void test_large_graph(const char *exe)
{
    char graph[2048];
    int pos = 0;
    pos += snprintf(graph + pos, sizeof(graph) - pos, "15 14\n");
    for (int i = 0; i < 14; i++)
        pos += snprintf(graph + pos, sizeof(graph) - pos, "%d %d 1\n", i, i+1);
    pos += snprintf(graph + pos, sizeof(graph) - pos, "0 14\n");

    const char *path = write_tmp_file("t12", graph);
    char *out = capture_stdout(exe, path);

    char expected[512];
    int ep = 0;
    for (int i = 0; i <= 14; i++) {
        if (i > 0) ep += snprintf(expected + ep, sizeof(expected) - ep, " -> ");
        ep += snprintf(expected + ep, sizeof(expected) - ep, "%d", i);
    }
    ep += snprintf(expected + ep, sizeof(expected) - ep, "\n14");

    check("T12 - Chain of 15 nodes (max per spec), weight=14", out, expected);
    free(out);
}

/* ═══════════════════════════════════════════════════════════════════
 *  MILESTONE 6 - Lock-Integrity Validation Hooks
 *
 *  Call log_node_entry / log_node_exit from milestone5.c around the
 *  critical section (sem_wait / sem_post) to generate a time-stamped
 *  log.  Then call verify_lock_integrity() after the simulation ends
 *  to confirm no two travelers occupied the same node concurrently.
 *
 *  Log file location: /tmp/m6_node_access.log
 *  Log format per line:
 *    ENTER <traveler_id> <node_id> <timestamp>
 *    EXIT  <traveler_id> <node_id> <timestamp>
 * ═══════════════════════════════════════════════════════════════════ */

#define M6_LOG_FILE "/tmp/m6_node_access.log"
#define MAX_LOG_RECORDS 4096
#define MAX_NODES_TRACKED 64

/* get_timestamp - wall-clock seconds with nanosecond resolution */
double get_timestamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* log_node_entry - call immediately after sem_wait returns */
void log_node_entry(int traveler_id, int node_id, double timestamp)
{
    FILE *f = fopen(M6_LOG_FILE, "a");
    if (!f) {
        fprintf(stderr, "[tester] WARNING: cannot open log '%s'\n", M6_LOG_FILE);
        return;
    }
    fprintf(f, "ENTER %d %d %.9f\n", traveler_id, node_id, timestamp);
    fclose(f);
}

/* log_node_exit - call immediately before sem_post */
void log_node_exit(int traveler_id, int node_id, double timestamp)
{
    FILE *f = fopen(M6_LOG_FILE, "a");
    if (!f) {
        fprintf(stderr, "[tester] WARNING: cannot open log '%s'\n", M6_LOG_FILE);
        return;
    }
    fprintf(f, "EXIT  %d %d %.9f\n", traveler_id, node_id, timestamp);
    fclose(f);
}

/* verify_lock_integrity - parse log and check for overlapping intervals
 * Returns: 0 = no violations, >0 = number of violations, -1 = file error */
int verify_lock_integrity(const char *log_file_path)
{
    const char *path = log_file_path ? log_file_path : M6_LOG_FILE;

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[verify] ERROR: cannot open log file '%s'\n", path);
        return -1;
    }

    typedef struct { int traveler_id; int node_id; double entry_time; } Pending;
    typedef struct { int traveler_id; int node_id;
                     double entry_time; double exit_time; } Interval;

    static Pending  pending[MAX_LOG_RECORDS];
    static Interval intervals[MAX_LOG_RECORDS];
    int n_pending = 0, n_intervals = 0;

    char line[256];
    int line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        if (line[0] == '\n' || line[0] == '#') continue;

        char tag[8];
        int  traveler_id, node_id;
        double ts;

        if (sscanf(line, "%7s %d %d %lf", tag, &traveler_id, &node_id, &ts) != 4)
            continue;

        if (strcmp(tag, "ENTER") == 0) {
            if (n_pending < MAX_LOG_RECORDS)
                pending[n_pending++] = (Pending){ traveler_id, node_id, ts };
        } else if (strcmp(tag, "EXIT") == 0) {
            int matched = -1;
            for (int i = n_pending - 1; i >= 0; i--) {
                if (pending[i].traveler_id == traveler_id &&
                    pending[i].node_id     == node_id) {
                    matched = i; break;
                }
            }
            if (matched != -1 && n_intervals < MAX_LOG_RECORDS) {
                intervals[n_intervals++] = (Interval){
                    traveler_id, node_id,
                    pending[matched].entry_time, ts
                };
                pending[matched] = pending[--n_pending];
            }
        }
    }
    fclose(f);

    /* Sort intervals by entry_time */
    for (int i = 0; i < n_intervals - 1; i++)
        for (int j = i + 1; j < n_intervals; j++)
            if (intervals[j].entry_time < intervals[i].entry_time) {
                Interval tmp = intervals[i];
                intervals[i] = intervals[j];
                intervals[j] = tmp;
            }

    /* Check for overlaps per node */
    int node_ids[MAX_NODES_TRACKED];
    int n_nodes = 0;
    for (int i = 0; i < n_intervals; i++) {
        int nid = intervals[i].node_id, found = 0;
        for (int j = 0; j < n_nodes; j++)
            if (node_ids[j] == nid) { found = 1; break; }
        if (!found && n_nodes < MAX_NODES_TRACKED)
            node_ids[n_nodes++] = nid;
    }

    int total_violations = 0;
    for (int ni = 0; ni < n_nodes; ni++) {
        int nid = node_ids[ni];
        static Interval node_ivs[MAX_LOG_RECORDS];
        int cnt = 0;
        for (int i = 0; i < n_intervals; i++)
            if (intervals[i].node_id == nid)
                node_ivs[cnt++] = intervals[i];

        for (int i = 0; i < cnt - 1; i++) {
            if (node_ivs[i].exit_time > node_ivs[i+1].entry_time) {
                fprintf(stderr,
                    "[verify] VIOLATION node=%-3d traveler %d held [%.3f-%.3f]"
                    " overlaps traveler %d entry %.3f\n",
                    nid,
                    node_ivs[i].traveler_id,
                    node_ivs[i].entry_time,
                    node_ivs[i].exit_time,
                    node_ivs[i+1].traveler_id,
                    node_ivs[i+1].entry_time);
                total_violations++;
            }
        }
    }

    printf("[verify] Log file  : %s\n", path);
    printf("[verify] Intervals : %d  (across %d nodes)\n", n_intervals, n_nodes);
    if (total_violations == 0)
        printf("[verify] Result    : " GREEN "PASS" RESET
               " - no concurrent-node violations\n");
    else
        printf("[verify] Result    : " RED "FAIL" RESET
               " - %d violation(s) detected\n", total_violations);

    return total_violations;
}

/* ═══════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════ */
int run_test(int argc, char *argv[])
{
    const char *exe = (argc >= 2) ? argv[1] : "./Project";

    printf(YELLOW
           "\n╔══════════════════════════════════════════════╗\n"
           "║   OS Project - Milestone 1 Test Suite       ║\n"
           "║   Executable: %-28s  ║\n"
           "╚══════════════════════════════════════════════╝\n"
           RESET, exe);

    test_assignment_example(exe);
    test_no_path(exe);
    test_src_equals_dst(exe);
    test_negative_weight(exe);
    test_single_node(exe);
    test_weight_vs_hops(exe);
    test_long_detour(exe);
    test_two_nodes(exe);
    test_directed_no_return(exe);
    test_missing_file(exe);
    test_no_args(exe);
    test_large_graph(exe);

    printf(YELLOW
           "\n╔══════════════════════════════════════════════╗\n"
           RESET);
    printf("  Results: " GREEN "%d PASSED" RESET " / " RED "%d FAILED"
           RESET " / %d TOTAL\n", g_pass, g_fail, g_total);
    printf(YELLOW
           "╚══════════════════════════════════════════════╝\n\n"
           RESET);

    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}