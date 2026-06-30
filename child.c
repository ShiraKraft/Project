#define _XOPEN_SOURCE 700
#include "child.h"
#include "child_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <semaphore.h>
#include <fcntl.h>

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
 * - the graph (Graph*)
 * - the traveler list (src and dst only – no path!)
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
        // ==========================================================
        // SYNCHRONIZATION: Access and lock the unique node semaphore
        // ==========================================================
        char sem_name[32];
        sprintf(sem_name, "/node_sem_%d", msg.current_node);
        
        /* Open or connect to the named semaphore for the current node (initially 1) */
        sem_t *node_sem = sem_open(sem_name, O_CREAT, 0666, 1);
        
        /* Block and wait here if another passenger process currently occupies this node */
        sem_wait(node_sem); 
        // ==========================================================

        /* Send current location update to parent via IPC pipe */
        /* Only executed AFTER successfully acquiring the node lock */
        write(write_fd, &msg, sizeof(IPC_Message));

        /* --- EXAM TASK A: Wait for parent's signal approval --- */
sigset_t sigset;
int sig;
sigemptyset(&sigset);
sigaddset(&sigset, SIGUSR1);
sigprocmask(SIG_BLOCK, &sigset, NULL);

sigwait(&sigset, &sig); 


        /* If destination reached, terminate the travel loop */
        if (msg.is_destination) {
            /* Release the node semaphore before exiting the loop */
            sem_post(node_sem);
            sem_close(node_sem);
            break;
        }

        /* Travel across the edge: W hops × 300 ms each */
        int W = weights[i];
        for (int hop = 0; hop < W; hop++) {
            usleep(300 * 1000); /* 300 ms */
        }

        /* Intermediate node wait time (1 second) */
        usleep(1000 * 1000); /* 1s */

        // ==========================================================
        // SYNCHRONIZATION: Departure from node - release the lock
        // ==========================================================
        /* Unlock the semaphore to allow the next waiting passenger to enter */
        sem_post(node_sem);  
        /* Close the local descriptor link to this named semaphore */
        sem_close(node_sem); 
        // ==========================================================
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
 * 1. Initialise pipes
 * 2. Fork one child per traveler – child calls run_child_m5
 * 3. Parent runs GUI loop reading IPC messages
 * 4. waitpid for all children
 * ════════════════════════════════════════════════════════════════════ */
 void run_milestone5(SimulationManager *sim, Graph *g, const NodeVisual *nodes, const char *filename)
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
            // Initialize named semaphore for synchronization before the child starts
            sem_t *sem = sem_open("/node_lock", O_CREAT, 0644, 1);
            
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

static volatile sig_atomic_t g_terminate = 0;
 
static void handle_termination(int sig)
{
    (void)sig;
    g_terminate = 1;
}

vvoid run_child(const int *path, int path_len, const int *weights)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
 
    // Open the semaphore
    sem_t *sem = sem_open("/node_lock", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    printf("[%d] started\n", (int)getpid());
    fflush(stdout);
 
    for (int i = 0; i < path_len - 1 && !g_terminate; i++) {
        int W = weights[i];
        
        // Critical Section
        sem_wait(sem);

        /* Travel across the edge: W hops × 300 ms each */
        for (int hop = 0; hop < W && !g_terminate; hop++)
            usleep(300 * 1000);
 
        /* Wait 1 second at every intermediate node (skip destination) */
        int is_destination = (i == path_len - 2);
        if (!is_destination && !g_terminate)
            usleep(1000 * 1000);
            
        sem_post(sem);
        // End Critical Section

        /* ─── EXAM TASK A: Wait for Parent Approval via Signal ─── */
        sigset_t sigset;
        int sig;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);
        sigprocmask(SIG_BLOCK, &sigset, NULL);
        
        sigwait(&sigset, &sig);
       
    }
 
    while (!g_terminate)
        pause();
        
    sem_close(sem);
    exit(EXIT_SUCCESS);
}
