#define _XOPEN_SOURCE 700
#include "child.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>

#include "graph.h"
#include "gui.h"
#include "milestone4.h"   /* SimulationManager, Traveler, colors, drawing */
#include "child.h"        /* run_child_m5 */
#include "parent_ipc.h"   /* init_ipc_infrastructure, read_status_from_child */
#include "child_ipc.h"    /* child_ipc_set_total_children */
#include "ipc_protocol.h" /* StatusMessage */

/* ════════════════════════════════════════════════════════════════════
 * Helper: skip blank lines and comment lines
 * ════════════════════════════════════════════════════════════════════ */
static bool is_skip_line(const char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    return (*line == '#' || *line == '\n' || *line == '\r' || *line == '\0');
}

/* ════════════════════════════════════════════════════════════════════
 * parse_input_file_m5
 *
 * Reads the input file and extracts:
 *   - the graph (Graph*)
 *   - the traveler list (src and dst only – no path!)
 *
 * Milestone 5: the parent does not compute paths. path_size stays 0.
 * ════════════════════════════════════════════════════════════════════ */
static bool parse_input_file_m5(const char *filename,
                                  Graph **g_out,
                                  SimulationManager *sim)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror(filename); return false; }

    char line[256];
    int num_nodes = 0, num_edges = 0;

    /* Read graph header */
    while (fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        if (sscanf(line, "%d %d", &num_nodes, &num_edges) == 2) break;
    }

    if (num_nodes <= 0) {
        fprintf(stderr, "parse error: bad graph header\n");
        fclose(fp); return false;
    }

    Graph *g = initGraph(num_nodes);
    if (!g) { fclose(fp); return false; }

    /* Read edges */
    int edges_read = 0;
    while (edges_read < num_edges && fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        int s, d, w;
        if (sscanf(line, "%d %d %d", &s, &d, &w) == 3) {
            addEdge(g, s, d, w);
            edges_read++;
        }
    }

    /* Read traveler count */
    int num_travelers = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        if (sscanf(line, "%d", &num_travelers) == 1) break;
    }

    if (num_travelers <= 0) {
        fprintf(stderr, "parse error: no travelers\n");
        freeGraph(g); fclose(fp); return false;
    }
    if (num_travelers > MAX_TRAVELERS)
        num_travelers = MAX_TRAVELERS;

    /* Read travelers – src and dst only, no path computation */
    sim->traveler_count = 0;
    int read = 0;
    while (read < num_travelers && fgets(line, sizeof(line), fp)) {
        if (is_skip_line(line)) continue;
        int src, dst;
        if (sscanf(line, "%d %d", &src, &dst) != 2) continue;

        Traveler *t = &sim->travelers[sim->traveler_count];
        memset(t, 0, sizeof(*t));
        t->src_node  = src;
        t->dest_node = dst;
        t->path_size = 0;    /* Child will compute the path! */
        t->is_active = false;
        t->pid       = 0;

        sim->traveler_count++;
        read++;
    }

    fclose(fp);
    *g_out = g;
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 * process_ipc_messages
 *
 * Reads messages from all children (non-blocking) and updates GUI + prints log.
 * Called every frame of the render loop.
 * ════════════════════════════════════════════════════════════════════ */
static void process_ipc_messages(SimulationManager *sim,
                                  const NodeVisual *nodes,
                                  int num_travelers)
{
    for (int i = 0; i < num_travelers; i++) {
        Traveler *t = &sim->travelers[i];
        if (!t->is_active) continue;

        StatusMessage msg;
        int ret = read_status_from_child(i, &msg);
        if (ret != 1) continue; /* no message available right now */

        /* Process finished message */
        if (msg.is_finished) {
            printf("[PID=%d] finished\n", (int)msg.pid);
            fflush(stdout);
            t->is_active = false;
            continue;
        }

        /* Process destination arrival message */
        if (msg.is_destination) {
            printf("[PID=%d] arrived at node %d | DESTINATION\n",
                   (int)msg.pid, msg.current_node);
            fflush(stdout);
            /* Update GUI position */
            t->current_pos = (Vector2){
                nodes[msg.current_node].x,
                nodes[msg.current_node].y
            };
            continue;
        }

        /* Process regular node arrival message */
        printf("[PID=%d] arrived at node %d | next node: %d\n",
               (int)msg.pid, msg.current_node, msg.next_node);
        fflush(stdout);

        /* Update GUI position */
        t->current_pos = (Vector2){
            nodes[msg.current_node].x,
            nodes[msg.current_node].y
        };
    }
}

/* ════════════════════════════════════════════════════════════════════
 * all_done – checks whether all travelers have finished
 * ════════════════════════════════════════════════════════════════════ */
static bool all_done(const SimulationManager *sim)
{
    for (int i = 0; i < sim->traveler_count; i++)
        if (sim->travelers[i].is_active) return false;
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 * run_milestone5
 *
 * Main simulation function for Milestone 5:
 *   1. Initialise pipes
 *   2. Fork one child per traveler – child calls run_child_m5
 *   3. Parent runs GUI loop reading IPC messages
 *   4. waitpid for all children
 * ════════════════════════════════════════════════════════════════════ */
static void run_milestone5(SimulationManager *sim,
                            Graph *g,
                            const NodeVisual *nodes,
                            const char *filename)
{
    int n = sim->traveler_count;

    /* ── 1. Initialise pipes ─────────────────────────────────────── */
    child_ipc_set_total_children(n);
    if (init_ipc_infrastructure(n) != 0) {
        fprintf(stderr, "Failed to init IPC\n");
        return;
    }

    /* ── 2. Set initial GUI positions ───────────────────────────── */
    for (int i = 0; i < n; i++) {
        Traveler *t = &sim->travelers[i];
        t->current_pos = (Vector2){
            nodes[t->src_node].x,
            nodes[t->src_node].y
        };
        t->is_active = true;
    }

    /* ── 3. Fork one child per traveler ─────────────────────────── */
    for (int i = 0; i < n; i++) {
        Traveler *t = &sim->travelers[i];

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            t->is_active = false;
            continue;
        }

        if (pid == 0) {
            /* ── CHILD: computes path and sends IPC messages ─────── */
            run_child_m5(t->src_node, t->dest_node,
                         filename, i, n);
            /* run_child_m5 never returns */
            exit(EXIT_FAILURE);
        }

        /* ── PARENT: record PID ──────────────────────────────────── */
        t->pid = pid;
    }

    /* Parent closes the write ends of all pipes (belong to children only) */
    close_parent_write_ends();

    /* ── 4. GUI loop + IPC polling ──────────────────────────────── */
    while (!WindowShouldClose()) {

        /* Read messages from children */
        process_ipc_messages(sim, nodes, n);

        /* Drawing ──────────────────────────────────────────────────── */
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });

        /* Draw static graph */
        for (int u = 0; u < g->vertices; u++) {
            Node *cur = g->adjList[u];
            while (cur) {
                int v = cur->dest;
                Vector2 from = { nodes[u].x, nodes[u].y };
                Vector2 to   = { nodes[v].x, nodes[v].y };
                float dx = to.x - from.x, dy = to.y - from.y;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > 1e-4f) {
                    float r = nodes[v].radius;
                    to.x -= (dx/len)*r;
                    to.y -= (dy/len)*r;
                }
                extern void DrawDirectedEdge(Vector2, Vector2, int);
                DrawDirectedEdge(from, to, cur->weight);
                cur = cur->next;
            }
        }
        for (int i = 0; i < g->vertices; i++) {
            extern void DrawGraphNode(NodeVisual);
            DrawGraphNode(nodes[i]);
        }

        /* Draw travelers */
        DrawActiveTravelers(sim);

        /* HUD */
        DrawText("Graph Simulation - Milestone 5",
                 10, 10, 18, (Color){180, 180, 180, 255});
        DrawFPS(10, 34);

        EndDrawing();

        /* Early exit if all travelers finished */
        if (all_done(sim)) {
            WaitTime(1.5);
            break;
        }
    }

    /* ── 5. waitpid for all children ────────────────────────────── */
    for (int i = 0; i < n; i++) {
        Traveler *t = &sim->travelers[i];
        if (t->pid <= 0) continue;
        int status;
        waitpid(t->pid, &status, 0);
        t->pid = 0;
    }

    cleanup_ipc_infrastructure();
}

/* ════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];

    /* ── Read input file ─────────────────────────────────────────── */
    Graph *g = NULL;
    SimulationManager sim;
    memset(&sim, 0, sizeof(sim));

    if (!parse_input_file_m5(filename, &g, &sim)) {
        fprintf(stderr, "Failed to parse: %s\n", filename);
        return EXIT_FAILURE;
    }

    printf("[parent] loaded %d nodes, %d travelers\n",
           g->vertices, sim.traveler_count);

    /* ── Build graph layout ──────────────────────────────────────── */
    NodeVisual nodes[MAX_NODES];
    for (int i = 0; i < g->vertices; i++) {
        nodes[i].id     = i;
        nodes[i].radius = 24.0f;
        nodes[i].color  = (Color){70, 130, 180, 255};
        snprintf(nodes[i].label, sizeof(nodes[i].label), "%d", i);
    }
    Vector2 center = {600.0f, 400.0f};
    CalculateCircularLayout(nodes, g->vertices, center, 280.0f);

    /* ── Assign traveler colors ──────────────────────────────────── */
    InitializeTravelerColors(&sim);

    /* ── Open window ─────────────────────────────────────────────── */
    InitGraphWindow(1200, 800, "OS Project - Milestone 5");

    /* ── Run simulation ──────────────────────────────────────────── */
    run_milestone5(&sim, g, nodes, filename);

    /* ── Cleanup ─────────────────────────────────────────────────── */
    CloseWindow();
    freeGraph(g);

    printf("[parent] done\n");
    return EXIT_SUCCESS;
}
