/*
 * main_m4.c  –  Entry point for Milestone 4
 *
 * Reads the extended input file (graph + traveler list), computes Dijkstra
 * paths for each traveler, forks child processes, and launches the Raylib
 * GUI loop.  All process management and rendering is delegated to the
 * functions in milestone4.c.
 *
 * Input file format
 * ─────────────────
 *   # comment lines are ignored
 *   <num_nodes> <num_edges>
 *   <src> <dst> <weight>      (one edge per line, repeated num_edges times)
 *   # travelers
 *   <num_travelers>
 *   <src_node> <dst_node>     (one traveler per line)
 *
 * Build:
 *   make milestone4
 *
 * Run:
 *   ./sim <input_file>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "graph.h"        /* Graph, addEdge, dijkstra, freeGraph …  */
#include "gui.h"          /* NodeVisual, InitGraphWindow, …          */
#include "milestone4.h"   /* SimulationManager, Traveler, …          */

/* ── Parse helpers ──────────────────────────────────────────────────────── */

/* Skip blank lines and comment lines (start with '#') */
static bool is_skip_line(const char* line)
{
    while (*line == ' ' || *line == '\t') line++;
    return (*line == '#' || *line == '\n' || *line == '\r' || *line == '\0');
}

/* ─────────────────────────────────────────────────────────────────────────
 * parse_input_file
 * Fills *g (heap-allocated Graph) and *sim with the data from filename.
 * Returns true on success, false on any parse/IO error.
 * ───────────────────────────────────────────────────────────────────────── */
static bool parse_input_file(const char*        filename,
                              Graph**            g_out,
                              SimulationManager* sim)
{
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror(filename);
        return false;
    }

    char line[256];
    int  numNodes = 0, numEdges = 0;

    /* ── Read graph header (skip comments) ─────────────────────────────── */
    while (fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        if (sscanf(line, "%d %d", &numNodes, &numEdges) == 2) break;
    }
    if (numNodes <= 0 || numEdges < 0) {
        fprintf(stderr, "parse error: bad graph header\n");
        fclose(fp);
        return false;
    }

    Graph* g = initGraph(numNodes);
    if (!g) { fclose(fp); return false; }

    /* ── Read edges ─────────────────────────────────────────────────────── */
    int edgesRead = 0;
    while (edgesRead < numEdges && fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        int s, d, w;
        if (sscanf(line, "%d %d %d", &s, &d, &w) == 3) {
            addEdge(g, s, d, w);
            edgesRead++;
        }
    }

    /* ── Read traveler count ────────────────────────────────────────────── */
    int numTravelers = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        if (sscanf(line, "%d", &numTravelers) == 1) break;
    }
    if (numTravelers <= 0) {
        fprintf(stderr, "parse error: no travelers found\n");
        freeGraph(g);
        fclose(fp);
        return false;
    }
    if (numTravelers > MAX_TRAVELERS) {
        fprintf(stderr, "warning: clamping %d travelers to %d\n",
                numTravelers, MAX_TRAVELERS);
        numTravelers = MAX_TRAVELERS;
    }

    /* ── Compute Dijkstra path for each traveler ─────────────────────────  */
    sim->traveler_count = 0;
    int travelersRead   = 0;

    while (travelersRead < numTravelers && fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;

        int src, dst;
        if (sscanf(line, "%d %d", &src, &dst) != 2) continue;

        Traveler* t = &sim->travelers[sim->traveler_count];
        memset(t, 0, sizeof(*t));

        t->src_node  = src;
        t->dest_node = dst;
        t->pid       = 0;
        t->is_active = false;

        /* Run Dijkstra from the parent's copy of the graph */
        int dist[MAX_NODES], prev[MAX_NODES];
        dijkstra(g, src, dist, prev);

        if (dist[dst] == INF) {
            fprintf(stderr,
                "warning: no path from %d to %d – traveler skipped\n",
                src, dst);
            travelersRead++;
            continue;
        }

        /* Reconstruct the path by walking prev[] backwards, then reverse */
        int tmp[MAX_NODES], tmpLen = 0;
        for (int cur = dst; cur != -1 && tmpLen < MAX_NODES; cur = prev[cur])
            tmp[tmpLen++] = cur;

        /* Reverse into t->path */
        t->path_size = tmpLen;
        for (int k = 0; k < tmpLen; k++)
            t->path[k] = tmp[tmpLen - 1 - k];

        sim->traveler_count++;
        travelersRead++;
    }

    fclose(fp);
    *g_out = g;
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
 * main
 * ───────────────────────────────────────────────────────────────────────── */
int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* ── 1. Parse input ───────────────────────────────────────────────── */
    Graph*            g   = NULL;
    SimulationManager sim;
    memset(&sim, 0, sizeof(sim));

    if (!parse_input_file(argv[1], &g, &sim)) {
        fprintf(stderr, "Failed to parse input file: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    printf("[parent] graph loaded: %d nodes, %d travelers\n",
           g->vertices, sim.traveler_count);

    /* ── 2. Build NodeVisual layout (circular) ─────────────────────────── */
    NodeVisual nodes[MAX_NODES];
    for (int i = 0; i < g->vertices; i++) {
        nodes[i].id     = i;
        nodes[i].radius = 24.0f;
        nodes[i].color  = (Color){ 70, 130, 180, 255 };
        snprintf(nodes[i].label, sizeof(nodes[i].label), "%d", i);
    }
    Vector2 center = { 600.0f, 400.0f };
    CalculateCircularLayout(nodes, g->vertices, center, 280.0f);

    /* ── 3. Assign unique colors to each traveler ──────────────────────── */
    InitializeTravelerColors(&sim);

    /* ── 4. Open the Raylib window ─────────────────────────────────────── */
    InitGraphWindow(1200, 800, "OS Project – Milestone 4");

    /* ── 5. Fork child processes ─────────────────────────────────────────  */
    OrchestrateChildProcesses(&sim);

    /* ── 6. Run the GUI loop (also calls WaitForAllChildren on exit) ────── */
    ManagementGuiLoop(&sim, g, nodes);

    /* ── 7. Close the Raylib window ───────────────────────────────────── */
    CloseWindow();

    /* ── 8. Free heap memory ──────────────────────────────────────────── */
    freeGraph(g);

    printf("[parent] simulation complete – all children reaped\n");
    return EXIT_SUCCESS;
}