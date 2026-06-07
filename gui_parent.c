/*
 * gui_parent.c  –  Milestone 5: Parent Process & GUI Architect
 *
 * Implements all six tasks assigned to the Parent Process:
 *   Task 1  – ParseExtendedInputFiles
 *   Task 2  – InitializeRaylibVisualEnvironment
 *   Task 3  – ImplementNonBlockingIPCMonitoring
 *   Task 4  – RenderMultiColorTravelerIndicators
 *   Task 5  – ExecuteCentralLoggingOutput
 *   Task 6  – SynchronizeProcessTerminations
 */

#define _POSIX_C_SOURCE 200809L

#include "gui_parent.h"
#include "graph.h"
#include "parent_ipc.h"
#include "ipc_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Internal helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Palette of visually distinct colors – one per traveler slot */
static const Color TRAVELER_PALETTE[MAX_TRAVELERS] = {
    {  0, 228, 255, 255},   /*  0 – cyan          */
    {255,  80,  80, 255},   /*  1 – red           */
    { 80, 255,  80, 255},   /*  2 – green         */
    {255, 200,   0, 255},   /*  3 – yellow        */
    {200,   0, 255, 255},   /*  4 – purple        */
    {255, 140,   0, 255},   /*  5 – orange        */
    {  0, 180, 255, 255},   /*  6 – sky blue      */
    {255,  20, 147, 255},   /*  7 – deep pink     */
    {  0, 255, 160, 255},   /*  8 – spring green  */
    {255, 255,   0, 255},   /*  9 – pure yellow   */
    {180,  80, 255, 255},   /* 10 – violet        */
    {255, 100,  20, 255},   /* 11 – burnt orange  */
    { 20, 220, 180, 255},   /* 12 – teal          */
    {255,  60, 200, 255},   /* 13 – magenta       */
    {100, 200,  40, 255},   /* 14 – lime          */
    { 80, 120, 255, 255},   /* 15 – periwinkle    */
    /* slots 16-31: repeat with half-alpha so they remain distinct */
    {  0, 228, 255, 180}, {255,  80,  80, 180}, { 80, 255,  80, 180},
    {255, 200,   0, 180}, {200,   0, 255, 180}, {255, 140,   0, 180},
    {  0, 180, 255, 180}, {255,  20, 147, 180}, {  0, 255, 160, 180},
    {255, 255,   0, 180}, {180,  80, 255, 180}, {255, 100,  20, 180},
    { 20, 220, 180, 180}, {255,  60, 200, 180}, {100, 200,  40, 180},
    { 80, 120, 255, 180},
};

/* Background / UI colours (same style as gui.c) */
#define CLR_BG        ((Color){ 10,  14,  26, 255})
#define CLR_GRID      ((Color){ 20,  28,  48, 255})
#define CLR_NODE      ((Color){ 30, 160,  80, 255})
#define CLR_EDGE      ((Color){100, 140, 200, 150})
#define CLR_TEXT_INFO ((Color){180, 200, 220, 255})
#define CLR_TITLE     ((Color){  0, 220, 255, 255})

/* ═══════════════════════════════════════════════════════════════════
 *  Task 1 – ParseExtendedInputFiles
 * ═══════════════════════════════════════════════════════════════════ */
void ParseExtendedInputFiles(ParentSimulation *sim,
                             const char *matrix_file,
                             const char *travelers_file)
{
    if (!sim || !matrix_file) {
        fprintf(stderr, "[parent] ParseExtendedInputFiles: NULL argument\n");
        exit(EXIT_FAILURE);
    }

    /*
     * Milestone 5 uses a SINGLE unified input file containing:
     *   <num_nodes> <num_edges>
     *   <src> <dst> <weight>   (x num_edges)
     *   <num_travelers>
     *   <src> <dst>            (x num_travelers)
     *
     * Both matrix_file and travelers_file point to the same path.
     * We open the file once and read everything sequentially.
     * Comment lines starting with '#' are skipped.
     */
    (void)travelers_file; /* same file – ignored */

    FILE *fp = fopen(matrix_file, "r");
    if (!fp) {
        perror("[parent] fopen input file");
        exit(EXIT_FAILURE);
    }

    char line[256];

    /* ── 1a. Read graph header ────────────────────────────────────── */
    int num_nodes = 0, num_edges = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* skip blank lines and comment lines */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        if (sscanf(line, "%d %d", &num_nodes, &num_edges) == 2) break;
    }
    if (num_nodes <= 0 || num_edges < 0) {
        fprintf(stderr, "[parent] parse error: bad graph header\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    /* ── 1b. Allocate and fill the graph ─────────────────────────── */
    sim->graph = initGraph(num_nodes);
    if (!sim->graph) {
        fprintf(stderr, "[parent] initGraph failed\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    int edges_read = 0;
    while (edges_read < num_edges && fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        int s, d, w;
        if (sscanf(line, "%d %d %d", &s, &d, &w) == 3) {
            addEdge(sim->graph, s, d, w);
            edges_read++;
        }
    }

    /* ── 1c. Read traveler count ──────────────────────────────────── */
    int n = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        if (sscanf(line, "%d", &n) == 1) break;
    }
    if (n <= 0 || n > MAX_TRAVELERS) {
        fprintf(stderr,
            "[parent] invalid traveler count (%d). Must be 1..%d\n",
            n, MAX_TRAVELERS);
        fclose(fp);
        freeGraph(sim->graph);
        sim->graph = NULL;
        exit(EXIT_FAILURE);
    }
    sim->total_travelers = n;

    /* ── 1d. Read each traveler's src / dst ──────────────────────── */
    int travelers_read = 0;
    while (travelers_read < n && fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        int s = 0, d = 0;
        if (sscanf(line, "%d %d", &s, &d) != 2) continue;

        if (s < 0 || s >= sim->graph->vertices ||
            d < 0 || d >= sim->graph->vertices) {
            fprintf(stderr,
                "[parent] traveler %d out-of-range node "
                "(src=%d, dst=%d, vertices=%d)\n",
                travelers_read, s, d, sim->graph->vertices);
            fclose(fp);
            freeGraph(sim->graph);
            sim->graph = NULL;
            exit(EXIT_FAILURE);
        }

        sim->traveler_entries[travelers_read].src = s;
        sim->traveler_entries[travelers_read].dst = d;

        /* Pre-fill ChildTraveler slot */
        sim->travelers[travelers_read].src          = s;
        sim->travelers[travelers_read].dst          = d;
        sim->travelers[travelers_read].current_node = s;
        sim->travelers[travelers_read].next_node    = -1;
        sim->travelers[travelers_read].is_alive     = false;
        sim->travelers[travelers_read].pid          = -1;

        travelers_read++;
    }

    fclose(fp);
    printf("[parent] Loaded graph (%d nodes, %d edges) and %d traveler(s).\n",
           sim->graph->vertices, edges_read, sim->total_travelers);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════════════
 *  InitializeTravelerColors
 * ═══════════════════════════════════════════════════════════════════ */
void InitializeTravelerColors(ParentSimulation *sim)
{
    if (!sim) return;
    for (int i = 0; i < sim->total_travelers; i++) {
        sim->travelers[i].color = TRAVELER_PALETTE[i % MAX_TRAVELERS];
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 2 – InitializeRaylibVisualEnvironment
 * ═══════════════════════════════════════════════════════════════════ */
void InitializeRaylibVisualEnvironment(ParentSimulation *sim)
{
    if (!sim || !sim->graph) {
        fprintf(stderr, "[parent] InitializeRaylibVisualEnvironment: "
                        "sim or graph is NULL\n");
        exit(EXIT_FAILURE);
    }

    /* Open the Raylib window */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
               "Milestone 5 – Multi-Traveler Simulation");
    SetTargetFPS(TARGET_FPS);

    /* Pre-calculate screen positions for all graph nodes using a
     * circular layout identical to CalculateCircularLayout() in gui.c */
    int   numNodes = sim->graph->vertices;
    float cx       = SCREEN_WIDTH  / 2.0f;
    float cy       = SCREEN_HEIGHT / 2.0f;
    float radius   = (float)(SCREEN_HEIGHT < SCREEN_WIDTH
                             ? SCREEN_HEIGHT : SCREEN_WIDTH) * 0.36f;

    for (int i = 0; i < numNodes; i++) {
        float angle = (float)i * (2.0f * PI / (float)numNodes) - (PI / 2.0f);
        sim->node_screen_pos[i].x = cx + radius * cosf(angle);
        sim->node_screen_pos[i].y = cy + radius * sinf(angle);
    }

    /* Set the initial screen position of each traveler to its source node */
    for (int i = 0; i < sim->total_travelers; i++) {
        int src = sim->travelers[i].src;
        sim->travelers[i].position = sim->node_screen_pos[src];
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 3 – ImplementNonBlockingIPCMonitoring
 * ═══════════════════════════════════════════════════════════════════ */
void ImplementNonBlockingIPCMonitoring(ParentSimulation *sim)
{
    if (!sim) return;

    StatusMessage msg;

    for (int i = 0; i < sim->total_travelers; i++) {
        /* Skip children that already finished */
        if (!sim->travelers[i].is_alive) continue;

        /* Non-blocking read – returns immediately if no data */
        int result = read_status_from_child(i, &msg);

        if (result != 1) {
            /* 0 = no data yet (EAGAIN)  |  -1 = error or EOF */
            if (result == -1) {
                /* Pipe broken / child exited without sending is_finished */
                sim->travelers[i].is_alive = false;
            }
            continue;
        }

        /* ── Message received ──────────────────────────────────── */
        ChildTraveler *t = &sim->travelers[i];

        /* Update the traveler's known node */
        t->current_node = msg.current_node;
        t->next_node    = msg.next_node;

        /* Update visual position to the screen coords of the current node */
        if (msg.current_node >= 0 &&
            msg.current_node < sim->graph->vertices) {
            t->position = sim->node_screen_pos[msg.current_node];
        }

        /* Log the event (parent is the ONLY one that prints) */
        if (msg.is_finished) {
            t->is_alive = false;
            ExecuteCentralLoggingOutput(t, LOG_EVENT_FINISHED, msg.current_node);
        } else if (msg.is_destination) {
            ExecuteCentralLoggingOutput(t, LOG_EVENT_DESTINATION, msg.current_node);
        } else {
            ExecuteCentralLoggingOutput(t, LOG_EVENT_ARRIVED, msg.current_node);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 4 – RenderMultiColorTravelerIndicators
 * ═══════════════════════════════════════════════════════════════════ */
void RenderMultiColorTravelerIndicators(const ParentSimulation *sim)
{
    if (!sim || !sim->graph) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    /* ── Background ──────────────────────────────────────────────── */
    ClearBackground(CLR_BG);

    /* Subtle grid */
    for (int x = 0; x < screenW; x += 40)
        DrawLine(x, 0, x, screenH, CLR_GRID);
    for (int y = 0; y < screenH; y += 40)
        DrawLine(0, y, screenW, y, CLR_GRID);

    /* ── Static graph: edges ─────────────────────────────────────── */
    for (int u = 0; u < sim->graph->vertices; u++) {
        Node *cur = sim->graph->adjList[u];
        while (cur) {
            int     v    = cur->dest;
            Vector2 from = sim->node_screen_pos[u];
            Vector2 to   = sim->node_screen_pos[v];

            /* Shorten the arrow so it ends at the node rim (radius 24) */
            float dx  = to.x - from.x;
            float dy  = to.y - from.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1e-4f) {
                float r = 24.0f;
                to.x -= (dx / len) * r;
                to.y -= (dy / len) * r;
            }

            /* Draw edge line */
            DrawLineV(from, to, CLR_EDGE);

            /* Arrow head */
            float arrowLen   = 12.0f;
            float arrowAngle = 25.0f * DEG2RAD;
            float ux = -(dx / len), uy = -(dy / len);
            float cosA = cosf(arrowAngle), sinA = sinf(arrowAngle);
            Vector2 w1 = { to.x + arrowLen * (ux*cosA - uy*sinA),
                           to.y + arrowLen * (ux*sinA + uy*cosA) };
            Vector2 w2 = { to.x + arrowLen * (ux*cosA + uy*sinA),
                           to.y + arrowLen * (-ux*sinA + uy*cosA) };
            DrawLineV(to, w1, CLR_EDGE);
            DrawLineV(to, w2, CLR_EDGE);

            /* Edge weight label */
            Vector2 mid  = { (from.x + to.x) / 2.0f,
                             (from.y + to.y) / 2.0f };
            char wlabel[16];
            snprintf(wlabel, sizeof(wlabel), "%d", cur->weight);
            int fs    = 13;
            int textW = MeasureText(wlabel, fs);
            DrawRectangle((int)(mid.x - textW/2 - 3),
                          (int)(mid.y - fs/2 - 2),
                          textW + 6, fs + 4,
                          (Color){30, 30, 30, 180});
            DrawText(wlabel,
                     (int)(mid.x - textW/2),
                     (int)(mid.y - fs/2),
                     fs, YELLOW);

            cur = cur->next;
        }
    }

    /* ── Static graph: nodes ─────────────────────────────────────── */
    for (int i = 0; i < sim->graph->vertices; i++) {
        Vector2 pos = sim->node_screen_pos[i];
        float   r   = 24.0f;

        /* Drop shadow */
        DrawCircle((int)pos.x + 3, (int)pos.y + 3, r,
                   (Color){0, 0, 0, 60});
        /* Node body */
        DrawCircleV(pos, r, CLR_NODE);
        DrawCircleLines((int)pos.x, (int)pos.y, (int)r, WHITE);

        /* Node index label */
        char label[8];
        snprintf(label, sizeof(label), "%d", i);
        int fs    = 16;
        int textW = MeasureText(label, fs);
        DrawText(label,
                 (int)(pos.x - textW / 2),
                 (int)(pos.y - fs / 2),
                 fs, WHITE);
    }

    /* ── Dynamic traveler indicators ─────────────────────────────── */
    for (int i = 0; i < sim->total_travelers; i++) {
        const ChildTraveler *t = &sim->travelers[i];
        if (!t->is_alive) continue;

        Vector2 pos   = t->position;
        Color   color = t->color;
        float   r     = 14.0f;

        /* Pulsing glow */
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f + (float)i);
        float glowR = r + 4.0f + pulse * 4.0f;
        DrawCircleV(pos, glowR,
                    (Color){color.r, color.g, color.b, 50});

        /* Traveler dot */
        DrawCircleV(pos, r, color);
        DrawCircleLines((int)pos.x, (int)pos.y, (int)r, WHITE);

        /* Index label inside the dot */
        char tidx[4];
        snprintf(tidx, sizeof(tidx), "%d", i);
        int fs    = 11;
        int textW = MeasureText(tidx, fs);
        DrawText(tidx,
                 (int)(pos.x - textW / 2),
                 (int)(pos.y - fs / 2),
                 fs, WHITE);

        /* PID tooltip above the dot */
        if (t->pid > 0) {
            char pidlabel[32];
            snprintf(pidlabel, sizeof(pidlabel), "PID:%d", (int)t->pid);
            int plw = MeasureText(pidlabel, 10);
            DrawText(pidlabel,
                     (int)(pos.x - plw / 2),
                     (int)(pos.y - r - 14),
                     10, color);
        }
    }

    /* ── HUD / Title ─────────────────────────────────────────────── */
    DrawText("Milestone 5 – Multi-Traveler Simulation",
             10, 10, 18, CLR_TITLE);
    DrawFPS(10, 34);

    /* Legend: traveler colors */
    int legendY = 60;
    DrawText("Travelers:", 10, legendY, 13, CLR_TEXT_INFO);
    legendY += 16;
    for (int i = 0; i < sim->total_travelers; i++) {
        const ChildTraveler *t = &sim->travelers[i];
        char entry[64];
        snprintf(entry, sizeof(entry),
                 "[%d] PID:%-6d  %d→%d  node:%d  %s",
                 i,
                 (int)t->pid,
                 t->src, t->dst,
                 t->current_node,
                 t->is_alive ? "running" : "done");
        DrawRectangle(8, legendY - 1, 10, 12, t->color);
        DrawText(entry, 22, legendY, 12, t->is_alive ? WHITE : GRAY);
        legendY += 15;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 5 – ExecuteCentralLoggingOutput
 * ═══════════════════════════════════════════════════════════════════ */
void ExecuteCentralLoggingOutput(const ChildTraveler *updater,
                                 int event_type,
                                 int node_id)
{
    if (!updater) return;

    switch (event_type) {
        case LOG_EVENT_ARRIVED:
            printf("[%d] arrived at node %d | next node: %d\n",
                   (int)updater->pid,
                   node_id,
                   updater->next_node);
            break;

        case LOG_EVENT_DESTINATION:
            printf("[%d] arrived at node %d | DESTINATION\n",
                   (int)updater->pid,
                   node_id);
            break;

        case LOG_EVENT_FINISHED:
            printf("[%d] finished\n", (int)updater->pid);
            break;

        default:
            printf("[%d] unknown event at node %d\n",
                   (int)updater->pid, node_id);
            break;
    }

    fflush(stdout);   /* guarantee live output visibility */
}

/* ═══════════════════════════════════════════════════════════════════
 *  Task 6 – SynchronizeProcessTerminations
 * ═══════════════════════════════════════════════════════════════════ */
void SynchronizeProcessTerminations(ParentSimulation *sim)
{
    if (!sim) return;

    printf("[parent] Waiting for all child processes to finish...\n");
    fflush(stdout);

    for (int i = 0; i < sim->total_travelers; i++) {
        pid_t pid = sim->travelers[i].pid;
        if (pid <= 0) continue;   /* never forked or already reaped */

        int   status = 0;
        pid_t result = waitpid(pid, &status, 0);   /* blocks until child exits */

        if (result == -1) {
            perror("[parent] waitpid");
            continue;
        }

        if (WIFEXITED(status)) {
            printf("[parent] child PID %d exited with code %d\n",
                   (int)pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[parent] child PID %d killed by signal %d\n",
                   (int)pid, WTERMSIG(status));
        }
        fflush(stdout);
    }

    printf("[parent] All children have terminated.\n");
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════════════
 *  FreeParentSimulation
 * ═══════════════════════════════════════════════════════════════════ */
void FreeParentSimulation(ParentSimulation *sim)
{
    if (!sim) return;
    if (sim->graph) {
        freeGraph(sim->graph);
        sim->graph = NULL;
    }
}
