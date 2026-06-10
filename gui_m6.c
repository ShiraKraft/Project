/*
 * gui_m6.c  –  Milestone 6: Node Access Synchronisation – GUI Implementation
 *
 * Implements the three new functions declared in gui_m6.h:
 *
 *   CalculateWaitingPosition  – queue geometry math
 *   UpdateTravelersPositions  – per-frame position dispatch by state
 *   RenderSimulationFrame     – full visual render pass with sync feedback
 *
 * Link this file together with gui_parent.c (Milestone 5) and the rest
 * of the project.  The Milestone 5 functions (ParseExtendedInputFiles,
 * ImplementNonBlockingIPCMonitoring, etc.) remain in gui_parent.c
 * untouched; only the three render/update functions below are new.
 *
 * Build line example (adjust paths as needed):
 *   gcc -std=c11 -Wall -Wextra -o sim \
 *       main_m6.c gui_m6.c gui_parent.c parent_ipc.c child_ipc.c \
 *       milestone5.c milestone4.c graph.c Dijkstra.c signal_handler.c \
 *       -lraylib -lm
 */

#include "gui_m6.h"
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════════
 *  Colour constants  (same dark-tech palette used in gui.c / gui_parent.c)
 * ════════════════════════════════════════════════════════════════════════ */
#define CLR_BG           ((Color){ 10,  14,  26, 255})
#define CLR_GRID         ((Color){ 20,  28,  48, 255})
#define CLR_NODE_DEFAULT ((Color){ 30, 160,  80, 255})
#define CLR_NODE_LOCKED  ((Color){200,  60,  20, 255})  /* node holds a lock  */
#define CLR_EDGE         ((Color){100, 140, 200, 150})
#define CLR_WEIGHT_TXT   ((Color){200, 220, 255, 200})
#define CLR_NODE_LABEL   WHITE

/* Traveler state colours */
#define CLR_MOVING       WHITE                          /* edge fill outline  */
#define CLR_WAITING_FILL ((Color){255, 140,   0, 255})  /* orange             */
#define CLR_WAITING_RING ((Color){255, 200,   0, 220})  /* bright amber ring  */
#define CLR_INSIDE_RING  ((Color){ 50, 240,  80, 255})  /* pulsing green ring */

/* HUD */
#define CLR_HUD_BG       ((Color){  0,   0,   0, 160})
#define CLR_HUD_TXT      ((Color){220, 230, 255, 255})

/* Geometry */
#define NODE_RADIUS        24.0f
#define QUEUE_SPACING      28.0f   /* pixels between queued travelers         */
#define TRAVELER_RADIUS    10.0f
#define INSIDE_RADIUS      13.0f   /* slightly larger when inside node        */
#define RING_PULSE_SPEED    3.0f   /* radians/second for the inside-node ring */


/* ════════════════════════════════════════════════════════════════════════
 *  Internal helpers
 * ════════════════════════════════════════════════════════════════════════ */

/* Safe normalise: returns (0,0) if magnitude is negligible */
static Vector2 normalise(Vector2 v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len < 1e-4f) return (Vector2){0.0f, 0.0f};
    return (Vector2){ v.x / len, v.y / len };
}

/* Draw a directed edge with an arrowhead and weight label */
static void draw_directed_edge(Vector2 from, Vector2 to, int weight)
{
    /* Shorten tip so arrowhead ends at node rim */
    Vector2 d   = { to.x - from.x, to.y - from.y };
    float len   = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) return;

    Vector2 tip = { to.x - (d.x / len) * NODE_RADIUS,
                    to.y - (d.y / len) * NODE_RADIUS };

    DrawLineV(from, tip, CLR_EDGE);

    /* Arrowhead */
    float arrowLen   = 11.0f;
    float cosA = cosf(25.0f * DEG2RAD);
    float sinA = sinf(25.0f * DEG2RAD);
    float ux = -(d.x / len), uy = -(d.y / len);
    Vector2 w1 = { tip.x + arrowLen * (ux*cosA - uy*sinA),
                   tip.y + arrowLen * (ux*sinA + uy*cosA) };
    Vector2 w2 = { tip.x + arrowLen * (ux*cosA + uy*sinA),
                   tip.y + arrowLen * (-ux*sinA + uy*cosA) };
    DrawLineV(tip, w1, CLR_EDGE);
    DrawLineV(tip, w2, CLR_EDGE);

    /* Weight label at edge midpoint */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", weight);
    int   fz  = 13;
    int   tw  = MeasureText(buf, fz);
    Vector2 mid = { (from.x + tip.x) / 2.0f, (from.y + tip.y) / 2.0f };
    DrawText(buf, (int)(mid.x - tw / 2), (int)(mid.y - fz / 2), fz, CLR_WEIGHT_TXT);
}

/* Returns true if at least one traveler is STATE_INSIDE_NODE for this node */
static bool node_is_occupied(const ParentSimulation *sim, int node_id)
{
    for (int i = 0; i < sim->total_travelers; i++) {
        const ChildTraveler *t = &sim->travelers[i];
        if (t->is_alive &&
            t->visual_state == STATE_INSIDE_NODE &&
            t->target_node  == node_id) {
            return true;
        }
    }
    return false;
}

/* Returns number of travelers currently STATE_WAITING_OUTSIDE for node_id
 * that appear BEFORE index 'before_idx' in the travelers array.
 * Used to assign each waiter a stable queue index. */
static int count_waiters_before(const ParentSimulation *sim,
                                int node_id, int before_idx)
{
    int count = 0;
    for (int i = 0; i < before_idx; i++) {
        const ChildTraveler *t = &sim->travelers[i];
        if (t->is_alive &&
            t->visual_state == STATE_WAITING_OUTSIDE &&
            t->target_node  == node_id) {
            count++;
        }
    }
    return count;
}


/* ════════════════════════════════════════════════════════════════════════
 *  CalculateWaitingPosition
 * ════════════════════════════════════════════════════════════════════════ */
void CalculateWaitingPosition(Vector2 nodeCenter,
                              Vector2 prevNodeCenter,
                              int     waitingIndex,
                              float   nodeRadius,
                              Vector2 *outPosition)
{
    if (!outPosition) return;

    /*
     * Compute the direction from nodeCenter back toward prevNodeCenter.
     * This is the direction "along the incoming edge", which is where
     * the queue forms outside the node.
     */
    Vector2 raw_dir = { prevNodeCenter.x - nodeCenter.x,
                        prevNodeCenter.y - nodeCenter.y };
    Vector2 dir = normalise(raw_dir);

    /*
     * Place the traveler at:
     *   nodeCenter  +  dir * (nodeRadius + waitingIndex * QUEUE_SPACING)
     *
     * Index 0 → just outside the node rim.
     * Index 1 → one QUEUE_SPACING step further along the edge.
     * etc.
     */
    float offset = nodeRadius + (float)waitingIndex * QUEUE_SPACING;

    outPosition->x = nodeCenter.x + dir.x * offset;
    outPosition->y = nodeCenter.y + dir.y * offset;
}


/* ════════════════════════════════════════════════════════════════════════
 *  UpdateTravelersPositions
 * ════════════════════════════════════════════════════════════════════════ */
void UpdateTravelersPositions(ParentSimulation *sim)
{
    if (!sim) return;

    for (int i = 0; i < sim->total_travelers; i++) {
        ChildTraveler *t = &sim->travelers[i];
        if (!t->is_alive) continue;

        switch (t->visual_state) {

        /* ── Moving on edge ────────────────────────────────────────── */
        case STATE_MOVING_ON_EDGE:
            /*
             * The interpolated position is owned by the IPC monitor
             * (ImplementNonBlockingIPCMonitoring).  We leave it untouched.
             * The node_screen_pos[] lerp in the existing code already
             * handles this case correctly.
             */
            break;

        /* ── Waiting outside locked node ───────────────────────────── */
        case STATE_WAITING_OUTSIDE: {
            /*
             * Guard: target_node and prev_node must be valid graph indices.
             * If they haven't been set yet (e.g. right after fork before
             * any message arrived), skip silently.
             */
            if (t->target_node < 0 || t->target_node >= sim->graph->vertices)
                break;
            if (t->prev_node   < 0 || t->prev_node   >= sim->graph->vertices)
                break;

            Vector2 nodeCenter     = sim->node_screen_pos[t->target_node];
            Vector2 prevNodeCenter = sim->node_screen_pos[t->prev_node];

            /* Determine this traveler's position in the queue */
            int queue_index = count_waiters_before(sim, t->target_node, i);

            CalculateWaitingPosition(nodeCenter, prevNodeCenter,
                                     queue_index, NODE_RADIUS,
                                     &t->position);
            break;
        }

        /* ── Inside the node (holds the lock) ─────────────────────── */
        case STATE_INSIDE_NODE:
            if (t->target_node >= 0 &&
                t->target_node <  sim->graph->vertices) {
                /*
                 * Snap exactly to the node centre.  No interpolation
                 * needed; the traveler is stationary here for 1 second.
                 */
                t->position = sim->node_screen_pos[t->target_node];
            }
            break;
        }
    }
}


/* ════════════════════════════════════════════════════════════════════════
 *  RenderSimulationFrame
 * ════════════════════════════════════════════════════════════════════════ */
void RenderSimulationFrame(const ParentSimulation *sim)
{
    if (!sim || !sim->graph) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    /* ── 1. Background + grid ──────────────────────────────────────── */
    ClearBackground(CLR_BG);
    for (int x = 0; x < screenW; x += 40)
        DrawLine(x, 0, x, screenH, CLR_GRID);
    for (int y = 0; y < screenH; y += 40)
        DrawLine(0, y, screenW, y, CLR_GRID);

    /* ── 2. Edges ──────────────────────────────────────────────────── */
    for (int u = 0; u < sim->graph->vertices; u++) {
        Node *cur = sim->graph->adjList[u];
        while (cur) {
            draw_directed_edge(sim->node_screen_pos[u],
                               sim->node_screen_pos[cur->dest],
                               cur->weight);
            cur = cur->next;
        }
    }

    /* ── 3. Graph nodes ────────────────────────────────────────────── */
    for (int n = 0; n < sim->graph->vertices; n++) {
        Vector2 pos = sim->node_screen_pos[n];

        /* Choose fill colour: red if currently locked by a traveler */
        Color fill = node_is_occupied(sim, n)
                     ? CLR_NODE_LOCKED
                     : CLR_NODE_DEFAULT;

        /* Drop shadow */
        DrawCircle((int)pos.x + 3, (int)pos.y + 3,
                   NODE_RADIUS, (Color){0, 0, 0, 60});
        /* Fill */
        DrawCircleV(pos, NODE_RADIUS, fill);
        /* Outline */
        DrawCircleLines((int)pos.x, (int)pos.y, NODE_RADIUS, WHITE);

        /* Node ID label */
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n);
        int fz  = 16;
        int tw  = MeasureText(buf, fz);
        DrawText(buf, (int)(pos.x - tw / 2),
                      (int)(pos.y - fz / 2), fz, CLR_NODE_LABEL);
    }

    /* ── 4. Travelers ──────────────────────────────────────────────── */

    /*
     * Pulsing ring parameter – oscillates 0..1 using GetTime().
     * Used for the STATE_INSIDE_NODE ring animation.
     */
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * RING_PULSE_SPEED);
    float ring_extra = 4.0f + pulse * 5.0f;  /* ring radius grows ±5 px */

    for (int i = 0; i < sim->total_travelers; i++) {
        const ChildTraveler *t = &sim->travelers[i];
        if (!t->is_alive) continue;

        Vector2 pos = t->position;

        switch (t->visual_state) {

        /* ── STATE_MOVING_ON_EDGE ────────────────────────────────── */
        case STATE_MOVING_ON_EDGE:
            /* Drop shadow */
            DrawCircle((int)pos.x + 2, (int)pos.y + 2,
                       TRAVELER_RADIUS, (Color){0, 0, 0, 80});
            /* Filled circle in the traveler's unique colour */
            DrawCircleV(pos, TRAVELER_RADIUS, t->color);
            /* White outline */
            DrawCircleLines((int)pos.x, (int)pos.y,
                            (int)TRAVELER_RADIUS, WHITE);
            break;

        /* ── STATE_WAITING_OUTSIDE ───────────────────────────────── */
        case STATE_WAITING_OUTSIDE: {
            /*
             * Draw with ORANGE fill + amber outline ring to make the
             * "blocked and queued" state visually unmistakeable.
             */
            /* Shadow */
            DrawCircle((int)pos.x + 2, (int)pos.y + 2,
                       TRAVELER_RADIUS, (Color){0, 0, 0, 80});
            /* Orange fill */
            DrawCircleV(pos, TRAVELER_RADIUS, CLR_WAITING_FILL);
            /* Amber ring */
            DrawCircleLines((int)pos.x, (int)pos.y,
                            (int)(TRAVELER_RADIUS + 3), CLR_WAITING_RING);

            /*
             * "WAIT" text label above the dot so the instructor can
             * unambiguously see the queue has formed outside the node.
             */
            const char *lbl = "WAIT";
            int fz = 12;
            int tw = MeasureText(lbl, fz);
            DrawText(lbl,
                     (int)(pos.x - tw / 2),
                     (int)(pos.y - TRAVELER_RADIUS - fz - 2),
                     fz, CLR_WAITING_RING);

            /* Also draw the traveler's index/PID above the label */
            char pid_buf[16];
            snprintf(pid_buf, sizeof(pid_buf), "#%d", i);
            int tw2 = MeasureText(pid_buf, fz - 1);
            DrawText(pid_buf,
                     (int)(pos.x - tw2 / 2),
                     (int)(pos.y - TRAVELER_RADIUS - fz - 2 - (fz + 1)),
                     fz - 1, t->color);
            break;
        }

        /* ── STATE_INSIDE_NODE ───────────────────────────────────── */
        case STATE_INSIDE_NODE: {
            /*
             * Draw slightly larger and with a pulsing GREEN ring so the
             * instructor can confirm that exactly ONE traveler occupies
             * the node at a time (the ring makes it unmistakeable).
             */
            /* Shadow */
            DrawCircle((int)pos.x + 2, (int)pos.y + 2,
                       INSIDE_RADIUS, (Color){0, 0, 0, 80});
            /* Filled in traveler colour */
            DrawCircleV(pos, INSIDE_RADIUS, t->color);
            /* White inner outline */
            DrawCircleLines((int)pos.x, (int)pos.y,
                            (int)INSIDE_RADIUS, WHITE);
            /* Pulsing green outer ring */
            float ring_r = INSIDE_RADIUS + ring_extra;
            Color ring_c = CLR_INSIDE_RING;
            ring_c.a = (unsigned char)(180 + (int)(pulse * 75));
            DrawCircleLines((int)pos.x, (int)pos.y,
                            (int)ring_r, ring_c);

            /* Small "IN" text above */
            const char *lbl = "IN";
            int fz = 12;
            int tw = MeasureText(lbl, fz);
            DrawText(lbl,
                     (int)(pos.x - tw / 2),
                     (int)(pos.y - INSIDE_RADIUS - ring_extra - fz - 2),
                     fz, CLR_INSIDE_RING);
            break;
        }
        } /* end switch */
    }

    /* ── 5. HUD legend (bottom-left) ───────────────────────────────── */
    {
        int live_count = 0;
        for (int i = 0; i < sim->total_travelers; i++)
            if (sim->travelers[i].is_alive) live_count++;

        int hud_x  = 10;
        int hud_y  = screenH - sim->total_travelers * 22 - 50;
        int hud_w  = 320;
        int hud_h  = sim->total_travelers * 22 + 40;
        DrawRectangle(hud_x, hud_y, hud_w, hud_h, CLR_HUD_BG);
        DrawRectangleLines(hud_x, hud_y, hud_w, hud_h,
                           (Color){80, 100, 140, 180});

        /* Title */
        DrawText("Travelers [M6 Sync]",
                 hud_x + 8, hud_y + 6, 14, CLR_HUD_TXT);

        for (int i = 0; i < sim->total_travelers; i++) {
            const ChildTraveler *t = &sim->travelers[i];
            int row_y = hud_y + 26 + i * 22;

            /* Colour swatch */
            DrawRectangle(hud_x + 8, row_y + 3, 14, 14, t->color);

            /* State text */
            const char *state_str;
            Color state_col;
            switch (t->visual_state) {
            case STATE_WAITING_OUTSIDE:
                state_str = "WAITING"; state_col = CLR_WAITING_FILL; break;
            case STATE_INSIDE_NODE:
                state_str = "INSIDE";  state_col = CLR_INSIDE_RING;  break;
            default:
                state_str = t->is_alive ? "MOVING " : "DONE   ";
                state_col = t->is_alive ? CLR_HUD_TXT
                                        : (Color){120,120,120,200};
                break;
            }

            char line[64];
            if (t->is_alive) {
                snprintf(line, sizeof(line),
                         " T%-2d pid:%-6d  %s",
                         i, (int)t->pid, state_str);
            } else {
                snprintf(line, sizeof(line),
                         " T%-2d pid:%-6d  DONE",
                         i, (int)t->pid);
            }
            DrawText(line, hud_x + 26, row_y, 13, state_col);
        }

        /* Alive count */
        char summary[48];
        snprintf(summary, sizeof(summary), "Active: %d / %d",
                 live_count, sim->total_travelers);
        DrawText(summary, hud_x + 8, hud_y + hud_h - 18, 12, CLR_HUD_TXT);
    }

    /* ── Title bar ──────────────────────────────────────────────────── */
    DrawText("Milestone 6 – Node Access Synchronisation",
             10, 10, 18, (Color){160, 200, 255, 220});
}