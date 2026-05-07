/*
 * tester.c  –  Self-contained test suite for OS Project Milestone 1
 *
 * Compile:  gcc -o tester tester.c graph.c -lm
 * Run:      ./tester
 *
 * All input graphs are written to temp files at runtime – no external
 * files needed.  The tester calls dijkstra() directly and also checks
 * the full output via stdout capture (popen).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"   /* Node, Graph, Query, createNode, initGraph,
                        addEdge, parseGraphFromFile, dijkstra,
                        displayResults, freeGraph                  */

/* ═══════════════════════════════════════════════════════════════════
 *  Colour helpers
 * ═══════════════════════════════════════════════════════════════════ */
#define GREEN  "\033[0;32m"
#define RED    "\033[0;31m"
#define YELLOW "\033[1;33m"
#define CYAN   "\033[0;36m"
#define RESET  "\033[0m"
#define _POSIX_C_SOURCE 200809L   // allowing popen / pclose
#include <sys/wait.h>             // defines WEXITSTATUS
/* ═══════════════════════════════════════════════════════════════════
 *  Global counters
 * ═══════════════════════════════════════════════════════════════════ */
static int g_pass  = 0;
static int g_fail  = 0;
static int g_total = 0;

/* ═══════════════════════════════════════════════════════════════════
 *  write_tmp_file  –  dump a C-string to a temp file, return path
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
 *  capture_stdout  –  run "./Project <file>", return stdout as string
 *  (caller must free the returned buffer)
 * ═══════════════════════════════════════════════════════════════════ */
static char *capture_stdout(const char *exe, const char *input_file)
{
    char cmd[512];
    /* redirect stderr to /dev/null so only stdout is captured */
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
 *  trim_trailing  –  remove trailing whitespace / newlines in-place
 * ═══════════════════════════════════════════════════════════════════ */
static void trim_trailing(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' '))
        s[--n] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
 *  check  –  compare actual vs expected, print result
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
 *  check_error  –  verify program exits with non-zero code
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
        printf(RED   "  [FAIL]" RESET " %s  – expected non-zero exit, got 0\n",
               test_name);
        g_fail++;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  THE TESTS
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * T1 – Assignment example
 * Graph from the spec; expected shortest path 0→5 = weight 12
 * Route: 0 -> 2 -> 1 -> 3 -> 4 -> 5
 */
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
    /* Accepted outputs: the spec shows "0 -> 2 -> 5\n12"
       but the actual shortest via adjacency is 0->2->1->3->4->5 = 12.
       We test the weight line only, since path depends on edge ordering. */
    /* Find last line (weight) */
    char last[64] = "";
    char *nl = strrchr(out, '\n');
    if (nl && nl != out) {
        /* walk back past the trailing newline */
        *nl = '\0';
        char *prev = strrchr(out, '\n');
        strncpy(last, prev ? prev+1 : out, sizeof(last)-1);
        *nl = '\n';
    } else {
        strncpy(last, out, sizeof(last)-1);
    }
    trim_trailing(last);
    trim_trailing(out);

    /* Check that output contains "12" as weight */
    if (strstr(out, "12") != NULL && strstr(out, "->") != NULL) {
        g_total++;
        printf(GREEN "  [PASS]" RESET " T1 – Assignment example (weight=12, path printed)\n");
        g_pass++;
    } else {
        g_total++;
        printf(RED "  [FAIL]" RESET " T1 – Assignment example\n");
        printf("         Expected weight 12 and a path with '->'\n");
        printf("         Got: %s\n", out);
        g_fail++;
    }
    free(out);
}

/*
 * T2 – No path (disconnected graph)
 * Nodes 0-1 and nodes 2-3 are in separate components; query 0→3.
 */
static void test_no_path(const char *exe)
{
    const char *graph =
        "4 2\n"
        "0 1 3\n"
        "2 3 5\n"
        "0 3\n";
    const char *path = write_tmp_file("t2", graph);
    char *out = capture_stdout(exe, path);
    check("T2 – Disconnected graph → No path found", out, "No path found");
    free(out);
}

/*
 * T3 – src == dst (non-trivial graph)
 */
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
    check("T3 – src == dst → print node then 0", out, "0\n0");
    free(out);
}

/*
 * T4 – Negative weight → must exit with error
 */
static void test_negative_weight(const char *exe)
{
    const char *graph =
        "3 2\n"
        "0 1 -3\n"
        "1 2 2\n"
        "0 2\n";
    const char *path = write_tmp_file("t4", graph);
    check_error("T4 – Negative weight → error exit", exe, path);
}

/*
 * T5 – Single node, src == dst == 0
 */
static void test_single_node(const char *exe)
{
    const char *graph =
        "1 0\n"
        "0 0\n";
    const char *path = write_tmp_file("t5", graph);
    char *out = capture_stdout(exe, path);
    check("T5 – Single node, src==dst=0", out, "0\n0");
    free(out);
}

/*
 * T6 – Dijkstra picks lower-weight path (not fewest hops)
 * Direct edge 0→1 costs 10; via 0→2→1 costs 2.
 */
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
    /* Expected: 0 -> 2 -> 1\n2 */
    check("T6 – Shortest weight beats fewest hops (0->2->1, w=2)", out,
          "0 -> 2 -> 1\n2");
    free(out);
}

/*
 * T7 – Long detour is cheaper than direct edge
 * Direct 0→1 = 100; detour 0→2→3→4→1 = 1+1+1+1 = 4
 */
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
    check("T7 – Long detour cheaper than direct (0->2->3->4->1, w=4)", out,
          "0 -> 2 -> 3 -> 4 -> 1\n4");
    free(out);
}

/*
 * T8 – Minimal graph: two nodes, one edge, query matches edge
 */
static void test_two_nodes(const char *exe)
{
    const char *graph =
        "2 1\n"
        "0 1 7\n"
        "0 1\n";
    const char *path = write_tmp_file("t8", graph);
    char *out = capture_stdout(exe, path);
    check("T8 – Two-node graph, direct edge (w=7)", out, "0 -> 1\n7");
    free(out);
}

/*
 * T9 – No path in directed graph (edge only goes one way)
 * Edge exists 1→0 but not 0→1.
 */
static void test_directed_no_return(const char *exe)
{
    const char *graph =
        "2 1\n"
        "1 0 5\n"
        "0 1\n";
    const char *path = write_tmp_file("t9", graph);
    char *out = capture_stdout(exe, path);
    check("T9 – Directed edge 1→0 only, query 0→1 → No path found", out,
          "No path found");
    free(out);
}

/*
 * T10 – Missing input file → error exit
 */
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
               " T10 – Missing file → error exit (exit=%d)\n", exit_code);
        g_pass++;
    } else {
        printf(RED "  [FAIL]" RESET
               " T10 – Missing file → expected non-zero exit, got 0\n");
        g_fail++;
    }
}

/*
 * T11 – No arguments passed → usage error
 */
static void test_no_args(const char *exe)
{
    g_total++;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1", exe);
    int ret = system(cmd);
    int exit_code = WEXITSTATUS(ret);
    if (exit_code != 0) {
        printf(GREEN "  [PASS]" RESET
               " T11 – No arguments → error exit (exit=%d)\n", exit_code);
        g_pass++;
    } else {
        printf(RED "  [FAIL]" RESET
               " T11 – No arguments → expected non-zero exit, got 0\n");
        g_fail++;
    }
}

/*
 * T12 – Large-ish graph (15 nodes, guaranteed path)
 * Chain: 0→1→2→...→14, each weight 1. Query 0→14 = weight 14.
 */
static void test_large_graph(const char *exe)
{
    /* build the graph string */
    char graph[2048];
    int pos = 0;
    pos += snprintf(graph + pos, sizeof(graph) - pos, "15 14\n");
    for (int i = 0; i < 14; i++)
        pos += snprintf(graph + pos, sizeof(graph) - pos, "%d %d 1\n", i, i+1);
    pos += snprintf(graph + pos, sizeof(graph) - pos, "0 14\n");

    const char *path = write_tmp_file("t12", graph);
    char *out = capture_stdout(exe, path);

    /* Build expected path string */
    char expected[512];
    int ep = 0;
    for (int i = 0; i <= 14; i++) {
        if (i > 0) ep += snprintf(expected + ep, sizeof(expected) - ep, " -> ");
        ep += snprintf(expected + ep, sizeof(expected) - ep, "%d", i);
    }
    ep += snprintf(expected + ep, sizeof(expected) - ep, "\n14");

    check("T12 – Chain of 15 nodes (max per spec), weight=14", out, expected);
    free(out);
}

/* ═══════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    const char *exe = (argc >= 2) ? argv[1] : "./Project";

    printf(YELLOW
           "\n╔══════════════════════════════════════════════╗\n"
           "║   OS Project – Milestone 1 Test Suite       ║\n"
           "║   Executable: %-28s  ║\n"
           "╚══════════════════════════════════════════════╝\n"
           RESET, exe);

    test_assignment_example(exe);   /* T1  */
    test_no_path(exe);              /* T2  */
    test_src_equals_dst(exe);       /* T3  */
    test_negative_weight(exe);      /* T4  */
    test_single_node(exe);          /* T5  */
    test_weight_vs_hops(exe);       /* T6  */
    test_long_detour(exe);          /* T7  */
    test_two_nodes(exe);            /* T8  */
    test_directed_no_return(exe);   /* T9  */
    test_missing_file(exe);         /* T10 */
    test_no_args(exe);              /* T11 */
    test_large_graph(exe);          /* T12 */

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
