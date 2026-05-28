/*
 * milestone4.c  –  Milestone 4: Parent Process Architect & GUI Manager
 *
 * Implements all process-control and GUI functions declared in milestone4.h.
 *
 * Concurrency model
 * ─────────────────
 * The parent forks one child per traveler inside OrchestrateChildProcesses().
 * After that the parent re-enters C code (ManagementGuiLoop) and never blocks
 * waiting for a child — it renders at 60 FPS and signals children exactly when
 * their on-screen journey is finished.  Children run concurrently, sleeping in
 * run_child() (child.c) until they receive SIGUSR1.
 *
 * All waitpid() calls are deferred to WaitForAllChildren(), invoked once after
 * the Raylib window closes, to collect exit statuses and avoid zombie entries
 * in the process table.
 */

#include "milestone4.h"
#include "child.h"      /* run_child() – the child-process entry point       */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * traveler_edge_weight
 * Look up the weight of the directed edge (fromNode → toNode) in the
 * adjacency list.  Returns 1 as a safe fallback if the edge is not found
 * (should never happen with a well-formed graph + Dijkstra path).
 */
static int traveler_edge_weight(const Graph* g, int fromNode, int toNode)
{
    Node* cur = g->adjList[fromNode];
    while (cur) {
        if (cur->dest == toNode) return (cur->weight > 0 ? cur->weight : 1);
        cur = cur->next;
    }
    return 1;   /* fallback – prevents division-by-zero in interpolation */
}

/*
 * all_travelers_done
 * Returns true when every traveler in the simulation has is_active == false.
 * Used by ManagementGuiLoop to break early once all journeys are complete.
 */
static bool all_travelers_done(const SimulationManager* sim)
{
    for (int i = 0; i < sim->traveler_count; i++) {
        if (sim->travelers[i].is_active) return false;
    }
    return true;
}

/*
 * snap_traveler_to_node
 * Teleport a traveler's current_pos to the exact screen coordinates of a node.
 * Used at path segment boundaries and on arrival at destination.
 */
static void snap_traveler_to_node(Traveler* t, const NodeVisual* nodes, int nodeIdx)
{
    t->current_pos = (Vector2){ nodes[nodeIdx].x, nodes[nodeIdx].y };
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ExecuteChildLogic  (stub – real work done by run_child() in child.c)
 *
 * This function is called from the child branch of OrchestrateChildProcesses.
 * It builds the weights array that run_child() requires and delegates to it.
 * It never returns; run_child() calls exit() internally.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void ExecuteChildLogic(Traveler* t)
{
    /* The child only needs the path and its per-edge weights to "travel"
     * (i.e. to sleep for the correct wall-clock duration and wait for the
     * parent's termination signal).  We reconstruct the weights array here
     * from the Traveler's path, passing a dummy array of 1s because the
     * child process no longer has access to the parent's Graph object after
     * fork() on most architectures.  The real weights are used ONLY by the
     * parent for GUI interpolation; run_child() uses them for timing only.
     *
     * If you want the child to honour real edge-weight timing, pass the
     * actual weights – either pre-computed into a weights[] array inside
     * the Traveler struct (add it in milestone4.h) or through shared memory.
     * For Milestone 4 the spec states children "just sleep", so 1s per hop
     * is acceptable and keeps the struct lean.
     */
    int weights[MAX_PATH_NODES];
    memset(weights, 0, sizeof(weights));
    for (int i = 0; i < t->path_size - 1; i++)
        weights[i] = 1;   /* placeholder: 1 hop = 300 ms in run_child() */

    run_child(t->path, t->path_size, weights);
    /* run_child never returns; exit() is called inside */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  OrchestrateChildProcesses
 * ═══════════════════════════════════════════════════════════════════════════ */
void OrchestrateChildProcesses(SimulationManager* sim)
{
    if (!sim || sim->traveler_count <= 0) return;

    for (int i = 0; i < sim->traveler_count; i++) {
        Traveler* t = &sim->travelers[i];

        /* Skip travelers that have no valid path */
        if (t->path_size <= 0) {
            fprintf(stderr,
                "[parent] traveler %d has no path – skipping fork\n", i);
            t->is_active = false;
            continue;
        }

        /* ── fork() ──────────────────────────────────────────────────────── */
        pid_t pid = fork();

        if (pid < 0) {
            /* fork() failed – log and deactivate this traveler; keep going */
            perror("[parent] fork");
            fprintf(stderr,
                "[parent] traveler %d will not be simulated\n", i);
            t->is_active = false;
            continue;
        }

        if (pid == 0) {
            /* ── CHILD BRANCH ────────────────────────────────────────────── *
             * We are now the child process.  The Traveler struct was copied   *
             * by fork() so t->path / t->path_size are valid here.            *
             * ExecuteChildLogic registers the signal handler, prints           *
             * "[PID] started", then sleeps until SIGUSR1 arrives.             *
             * It never returns.                                               */
            ExecuteChildLogic(t);
            /* NOTREACHED */
            exit(EXIT_FAILURE);
        }

        /* ── PARENT BRANCH ───────────────────────────────────────────────── *
         * Record the child's PID and arm the traveler for animation.          */
        t->pid       = pid;
        t->is_active = true;

        printf("[parent] forked child PID %d for traveler %d "
               "(src=%d -> dst=%d)\n",
               (int)pid, i, t->src_node, t->dest_node);
        fflush(stdout);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TerminateChildProcess
 * ═══════════════════════════════════════════════════════════════════════════ */
void TerminateChildProcess(pid_t childPid)
{
    if (childPid <= 0) return;   /* never forked or already reaped */

    /* Send SIGUSR1 – this wakes up the child's pause() loop and causes it
     * to exit cleanly via exit(EXIT_SUCCESS) in run_child() (child.c).
     * SIGUSR1 is preferred over SIGKILL because it allows the child to
     * run any atexit() handlers and flush stdio buffers.                  */
    if (kill(childPid, SIGUSR1) == -1) {
        /* ESRCH means the process no longer exists – not an error */
        if (errno != ESRCH) {
            perror("[parent] kill(SIGUSR1)");
        }
    } else {
        printf("[parent] sent SIGUSR1 to child PID %d (journey complete)\n",
               (int)childPid);
        fflush(stdout);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WaitForAllChildren
 * ═══════════════════════════════════════════════════════════════════════════ */
void WaitForAllChildren(SimulationManager* sim)
{
    if (!sim) return;

    for (int i = 0; i < sim->traveler_count; i++) {
        Traveler* t = &sim->travelers[i];

        /* Only wait for travelers that were actually forked */
        if (t->pid <= 0) continue;

        /* If the child is still marked active (window was closed early,
         * not all journeys finished), terminate it first so waitpid()
         * does not block indefinitely.                                   */
        if (t->is_active) {
            TerminateChildProcess(t->pid);
            t->is_active = false;
        }

        /* ── waitpid: reap the child, discard exit status ─────────────── */
        int   status = 0;
        pid_t result = waitpid(t->pid, &status, 0);

        if (result == -1) {
            if (errno != ECHILD) {
                /* ECHILD = already reaped (e.g. via SIGCHLD handler) – OK */
                perror("[parent] waitpid");
            }
        } else {
            if (WIFEXITED(status)) {
                printf("[parent] child PID %d exited with status %d\n",
                       (int)t->pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[parent] child PID %d killed by signal %d\n",
                       (int)t->pid, WTERMSIG(status));
            }
            fflush(stdout);
        }

        t->pid = 0;   /* mark as reaped to prevent double-wait */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  InitializeTravelerColors
 * ═══════════════════════════════════════════════════════════════════════════ */
void InitializeTravelerColors(SimulationManager* sim)
{
    if (!sim) return;

    /*
     * A carefully chosen palette of 16 vivid, perceptually distinct colors.
     * Each pair has high contrast against the dark graph background (RGB ≈
     * 20-30, 20-30, 30-40) used in RenderFrame().  If there are more than
     * 16 travelers, the palette wraps around (mod 16).
     */
    static const Color PALETTE[] = {
        {  64, 224, 208, 255 },  /*  0 – turquoise       */
        { 255,  99,  71, 255 },  /*  1 – tomato red      */
        { 124, 252,   0, 255 },  /*  2 – lawn green      */
        { 255, 215,   0, 255 },  /*  3 – gold            */
        { 138,  43, 226, 255 },  /*  4 – blue-violet     */
        { 255, 165,   0, 255 },  /*  5 – orange          */
        {   0, 191, 255, 255 },  /*  6 – deep sky blue   */
        { 255,  20, 147, 255 },  /*  7 – deep pink       */
        { 127, 255,   0, 255 },  /*  8 – chartreuse      */
        { 255, 105, 180, 255 },  /*  9 – hot pink        */
        {   0, 255, 127, 255 },  /* 10 – spring green    */
        { 255,  69,   0, 255 },  /* 11 – orange-red      */
        { 173, 216, 230, 255 },  /* 12 – light blue      */
        { 240, 230, 140, 255 },  /* 13 – khaki           */
        { 218, 112, 214, 255 },  /* 14 – orchid          */
        {   0, 255, 255, 255 },  /* 15 – cyan            */
    };
    static const int PALETTE_SIZE = (int)(sizeof(PALETTE) / sizeof(PALETTE[0]));

    for (int i = 0; i < sim->traveler_count; i++) {
        sim->travelers[i].color = PALETTE[i % PALETTE_SIZE];
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  UpdateAllTravelersAnimation
 * ═══════════════════════════════════════════════════════════════════════════ */
void UpdateAllTravelersAnimation(SimulationManager*  sim,
                                 const Graph*        g,
                                 const NodeVisual*   nodes,
                                 float               dt)
{
    if (!sim || !g || !nodes || dt <= 0.0f) return;

    for (int i = 0; i < sim->traveler_count; i++) {
        Traveler* t = &sim->travelers[i];
        if (!t->is_active) continue;

        /* ── Guard: already at destination ──────────────────────────────── */
        if (t->current_path_index >= t->path_size - 1) {
            /* Snap to destination node (belt-and-braces) */
            snap_traveler_to_node(t, nodes, t->path[t->path_size - 1]);
            TerminateChildProcess(t->pid);
            t->is_active = false;
            continue;
        }

        /* ── State: pausing at an intermediate node ──────────────────────  */
        if (t->on_node_pause) {
            t->node_timer += dt;
            if (t->node_timer >= M4_NODE_WAIT_TIME) {
                /* Done waiting – resume travel on the next edge */
                t->node_timer    = 0.0f;
                t->on_node_pause = false;
                /* current_step was already reset to 0 when we entered pause */
            }
            /* No position update while paused */
            continue;
        }

        /* ── Accumulate movement timer ───────────────────────────────────  */
        t->move_timer += dt;

        /* Identify the current edge: path[current_path_index] → path[...+1] */
        int fromNode = t->path[t->current_path_index];
        int toNode   = t->path[t->current_path_index + 1];
        int W        = traveler_edge_weight(g, fromNode, toNode);

        /* ── Advance by one weight-unit segment every M4_STEP_INTERVAL s ── */
        while (t->move_timer >= M4_STEP_INTERVAL) {
            t->move_timer -= M4_STEP_INTERVAL;  /* preserve sub-tick remainder */
            t->current_step++;

            /* ── All W segments of this edge completed ───────────────────── */
            if (t->current_step >= W) {
                t->current_step = 0;
                t->current_path_index++;

                /* ── Reached the final destination? ─────────────────────── */
                if (t->current_path_index >= t->path_size - 1) {
                    snap_traveler_to_node(t, nodes,
                                         t->path[t->path_size - 1]);
                    /* Notify the child and deactivate */
                    TerminateChildProcess(t->pid);
                    t->is_active = false;
                    goto next_traveler;  /* break out of while + continue for */
                }

                /* ── Arrived at an intermediate node ────────────────────── */
                snap_traveler_to_node(t, nodes,
                                      t->path[t->current_path_index]);
                t->on_node_pause = true;
                t->node_timer    = 0.0f;
                t->move_timer    = 0.0f;  /* discard any leftover tick time   */
                goto next_traveler;
            }
        }

        /* ── Interpolate screen position along the current edge ─────────── */
        {
            Vector2 from = { nodes[fromNode].x, nodes[fromNode].y };
            Vector2 to   = { nodes[toNode].x,   nodes[toNode].y   };
            float   fW   = (float)W;
            float   t_   = (float)t->current_step / fW;

            t->current_pos.x = from.x + t_ * (to.x - from.x);
            t->current_pos.y = from.y + t_ * (to.y - from.y);
        }

        next_traveler:;   /* label for goto above – keeps the loop clean */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  DrawActiveTravelers
 * ═══════════════════════════════════════════════════════════════════════════ */
void DrawActiveTravelers(const SimulationManager* sim)
{
    if (!sim) return;

    for (int i = 0; i < sim->traveler_count; i++) {
        const Traveler* t = &sim->travelers[i];
        if (!t->is_active) continue;

        /* ── Outer glow ring (50 % alpha version of the traveler's color) ── */
        Color glow = t->color;
        glow.a     = 80;
        DrawCircleV(t->current_pos, M4_ENTITY_RADIUS + 5.0f, glow);

        /* ── Main body ───────────────────────────────────────────────────── */
        DrawCircleV(t->current_pos, M4_ENTITY_RADIUS, t->color);

        /* ── Inner highlight (white specular dot) ────────────────────────── */
        DrawCircleV(t->current_pos,
                    M4_ENTITY_RADIUS * 0.38f,
                    (Color){ 255, 255, 255, 110 });

        /* ── PID label above the circle ─────────────────────────────────── */
        if (t->pid > 0) {
            char label[24];
            snprintf(label, sizeof(label), "%d", (int)t->pid);
            int  fontSize = 12;
            int  textW    = MeasureText(label, fontSize);

            /* Small semi-transparent backdrop so the text is readable over
             * the graph edges                                               */
            DrawRectangle(
                (int)(t->current_pos.x - textW / 2 - 2),
                (int)(t->current_pos.y - M4_ENTITY_RADIUS - fontSize - 4),
                textW + 4,
                fontSize + 2,
                (Color){ 10, 10, 20, 160 }
            );
            DrawText(
                label,
                (int)(t->current_pos.x - textW / 2),
                (int)(t->current_pos.y - M4_ENTITY_RADIUS - fontSize - 3),
                fontSize,
                WHITE
            );
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ManagementGuiLoop
 * ═══════════════════════════════════════════════════════════════════════════ */
void ManagementGuiLoop(SimulationManager* sim,
                       const Graph*       g,
                       const NodeVisual*  nodes)
{
    if (!sim || !g || !nodes) return;

    /* ── Snap every traveler to its source node before the loop begins ───── */
    for (int i = 0; i < sim->traveler_count; i++) {
        Traveler* t = &sim->travelers[i];
        if (!t->is_active || t->path_size <= 0) continue;
        snap_traveler_to_node(t, nodes, t->path[0]);
        t->current_path_index = 0;
        t->current_step       = 0;
        t->move_timer         = 0.0f;
        t->node_timer         = 0.0f;
        t->on_node_pause      = false;
    }

    /* ── Main rendering loop ─────────────────────────────────────────────── */
    while (!WindowShouldClose()) {

        /* 1. Compute delta time (frame-rate-independent animation) */
        float dt = GetFrameTime();

        /* 2. Update all traveler positions */
        UpdateAllTravelersAnimation(sim, g, nodes, dt);

        /* 3. Draw frame ──────────────────────────────────────────────────── */
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });

        /* 3a. Draw static graph (edges + nodes) – reuse RenderFrame logic.
         *     We redraw manually here to keep control of the title string. */
        for (int u = 0; u < g->vertices; u++) {
            Node* cur = g->adjList[u];
            while (cur) {
                int v = cur->dest;

                Vector2 from = { nodes[u].x, nodes[u].y };
                Vector2 to   = { nodes[v].x, nodes[v].y };

                /* Shorten the line so the arrowhead meets the node border */
                float dx  = to.x - from.x;
                float dy  = to.y - from.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    float r = nodes[v].radius;
                    to.x -= (dx / len) * r;
                    to.y -= (dy / len) * r;
                }
                /* DrawDirectedEdge is defined in gui.c */
                extern void DrawDirectedEdge(Vector2, Vector2, int);
                DrawDirectedEdge(from, to, cur->weight);

                cur = cur->next;
            }
        }
        for (int i = 0; i < g->vertices; i++) {
            extern void DrawGraphNode(NodeVisual);
            DrawGraphNode(nodes[i]);
        }

        /* 3b. Draw all active travelers on top of the graph */
        DrawActiveTravelers(sim);

        /* 3c. HUD: milestone label + active-traveler count */
        DrawText("Graph Simulation – Milestone 4",
                 10, 10, 18, (Color){ 180, 180, 180, 255 });
        DrawFPS(10, 34);

        int activeCount = 0;
        for (int i = 0; i < sim->traveler_count; i++)
            if (sim->travelers[i].is_active) activeCount++;

        char hudBuf[64];
        snprintf(hudBuf, sizeof(hudBuf),
                 "Travelers active: %d / %d",
                 activeCount, sim->traveler_count);
        DrawText(hudBuf, 10, 58, 16, (Color){ 200, 200, 100, 255 });

        EndDrawing();

        /* 4. Early exit: all travelers have reached their destinations */
        if (all_travelers_done(sim)) {
            /* Brief pause so the user can see the final frame */
            WaitTime(1.2);
            break;
        }
    }

    /* 5. Reap all child processes before returning to main() */
    WaitForAllChildren(sim);
}