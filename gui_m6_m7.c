/*
 * gui_m6_m7.c  –  Milestone 7: Scheduler GUI Overlay
 *
 * Implements the two symbols declared in gui_m6_m7.h.
 * NO existing M1-M6 files are modified.
 *
 * Visual additions
 * ────────────────
 *  1. Top-right badge:   "Scheduler Mode: FCFS"  or  "Scheduler Mode: SJF"
 *  2. Per-node queue panel (right side): shows how many travelers are queued
 *     at each node and their ordered IDs (meaningful when SYNC_WAITING msgs
 *     have arrived).
 *  3. Entry-flash: when a traveler transitions WAITING → INSIDE_NODE its
 *     circle pulses WHITE for ~0.4 s using GetTime().
 *
 * All drawing happens INSIDE an already-open BeginDrawing()/EndDrawing()
 * block started by RenderSimulationFrame().  We therefore call
 * RenderSimulationFrame() first (which calls BeginDrawing/EndDrawing),
 * then open our own BeginDrawing() block for the overlay.
 *
 * NOTE: Raylib allows nested BeginDrawing()/EndDrawing() pairs – the inner
 * pair draws on top of whatever the outer pair placed on the render target.
 */

#include "gui_m6_m7.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Palette (M7 additions only) ───────────────────────────────────────── */
#define CLR_BADGE_BG      ((Color){  0,  20,  50, 210})
#define CLR_BADGE_BORDER  ((Color){  0, 200, 255, 200})
#define CLR_BADGE_TEXT    ((Color){  0, 240, 180, 255})
#define CLR_FCFS_ACCENT   ((Color){ 80, 180, 255, 255})   /* cool blue  */
#define CLR_SJF_ACCENT    ((Color){255, 160,  40, 255})   /* warm amber */
#define CLR_QUEUE_BG      ((Color){  8,  16,  36, 200})
#define CLR_QUEUE_BORDER  ((Color){ 60,  80, 140, 180})
#define CLR_QUEUE_TITLE   ((Color){160, 200, 255, 220})
#define CLR_QUEUE_ENTRY   ((Color){200, 220, 255, 200})
#define CLR_FLASH         ((Color){255, 255, 255, 200})

/* ── Layout constants ───────────────────────────────────────────────────── */
#define BADGE_MARGIN_RIGHT  16
#define BADGE_MARGIN_TOP    10
#define BADGE_PAD_X         14
#define BADGE_PAD_Y          8
#define BADGE_FONT_SIZE     20

#define QUEUE_PANEL_W      230
#define QUEUE_PANEL_X_PAD   10
#define QUEUE_ROW_H         18
#define QUEUE_FONT_SIZE     13
#define QUEUE_TITLE_SIZE    14

/* ── Flash duration (seconds) ───────────────────────────────────────────── */
#define FLASH_DURATION   0.40f

/* ═══════════════════════════════════════════════════════════════════════════
 *  Module-private state
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Pointer to e.g. "FCFS" or "SJF" – set once at startup */
static const char *g_sched_name = "FCFS";

/*
 * Per-traveler flash tracking.
 * When a traveler flips to STATE_INSIDE_NODE we record GetTime() so we can
 * compute alpha fade = 1 - elapsed/FLASH_DURATION for the next ~0.4 s.
 */
#define MAX_FLASH_SLOTS  32
static double  g_flash_start[MAX_FLASH_SLOTS];  /* GetTime() at transition */
static int     g_prev_state[MAX_FLASH_SLOTS];    /* previous visual_state   */

/* ═══════════════════════════════════════════════════════════════════════════
 *  SetSchedulerDisplayName
 * ═══════════════════════════════════════════════════════════════════════════ */
void SetSchedulerDisplayName(const char *name)
{
    g_sched_name = name ? name : "FCFS";
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  draw_scheduler_badge  (internal)
 *
 *  Draws the top-right HUD badge showing the active scheduler.
 *  Picks accent colour based on algorithm name.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void draw_scheduler_badge(int screenW)
{
    /* Build the label string */
    char label[64];
    snprintf(label, sizeof(label), "Scheduler Mode: %s", g_sched_name);

    int   fs      = BADGE_FONT_SIZE;
    int   textW   = MeasureText(label, fs);
    int   boxW    = textW + BADGE_PAD_X * 2;
    int   boxH    = fs    + BADGE_PAD_Y * 2;
    int   boxX    = screenW - boxW - BADGE_MARGIN_RIGHT;
    int   boxY    = BADGE_MARGIN_TOP;

    /* Background */
    DrawRectangle(boxX, boxY, boxW, boxH, CLR_BADGE_BG);
    DrawRectangleLines(boxX, boxY, boxW, boxH, CLR_BADGE_BORDER);

    /* Accent left-edge bar (4 px wide) */
    Color accent = (strncmp(g_sched_name, "SJF", 3) == 0)
                   ? CLR_SJF_ACCENT
                   : CLR_FCFS_ACCENT;
    DrawRectangle(boxX, boxY, 4, boxH, accent);

    /* Text */
    DrawText(label, boxX + BADGE_PAD_X, boxY + BADGE_PAD_Y, fs, CLR_BADGE_TEXT);

    /* ── Animated pulsing underline beneath badge ─────────────────────── */
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    Color uline = accent;
    uline.a     = (unsigned char)(100 + (int)(pulse * 100));
    DrawRectangle(boxX + 4, boxY + boxH, boxW - 4, 2, uline);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  draw_queue_panel  (internal)
 *
 *  Right-side panel showing active node queues.
 *  Only nodes with at least one waiting traveler are listed.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void draw_queue_panel(const ParentSimulation *sim,
                              const WaitingQueue     *node_queues,
                              int screenW, int screenH)
{
    if (!sim || !node_queues) return;

    /* Count how many nodes have waiting travelers */
    int active_nodes = 0;
    for (int n = 0; n < sim->graph->vertices; n++)
        if (node_queues[n].size > 0) active_nodes++;

    if (active_nodes == 0) return;  /* nothing to draw */

    /* Calculate total panel height dynamically */
    int total_rows = 0;
    for (int n = 0; n < sim->graph->vertices; n++)
        if (node_queues[n].size > 0)
            total_rows += 1 + node_queues[n].size;  /* header + entries */

    int panelH = total_rows * QUEUE_ROW_H + 30;   /* +30 for panel title */
    int panelX = screenW - QUEUE_PANEL_W - QUEUE_PANEL_X_PAD;
    int panelY = BADGE_MARGIN_TOP + BADGE_FONT_SIZE + BADGE_PAD_Y * 2 + 24;

    /* Clamp panel so it doesn't fall off screen */
    if (panelY + panelH > screenH - 10)
        panelH = screenH - 10 - panelY;
    if (panelH < 20) return;

    /* Panel background + border */
    DrawRectangle(panelX, panelY, QUEUE_PANEL_W, panelH, CLR_QUEUE_BG);
    DrawRectangleLines(panelX, panelY, QUEUE_PANEL_W, panelH, CLR_QUEUE_BORDER);

    /* Panel title */
    DrawText("Waiting Queues",
             panelX + 8, panelY + 6,
             QUEUE_TITLE_SIZE, CLR_QUEUE_TITLE);

    int cursor_y = panelY + 6 + QUEUE_TITLE_SIZE + 4;

    for (int n = 0; n < sim->graph->vertices && cursor_y < panelY + panelH - QUEUE_ROW_H; n++) {
        if (node_queues[n].size == 0) continue;

        /* Node header row */
        char header[48];
        snprintf(header, sizeof(header), " Node %d  [%d waiting]",
                 n, node_queues[n].size);
        DrawText(header, panelX + 8, cursor_y, QUEUE_FONT_SIZE,
                 CLR_FCFS_ACCENT);
        cursor_y += QUEUE_ROW_H;

        /* Entry rows (ordered = scheduling order) */
        for (int q = 0; q < node_queues[n].size &&
                        cursor_y < panelY + panelH - QUEUE_ROW_H; q++) {
            char entry[48];
            snprintf(entry, sizeof(entry),
                     "  [%d] pid:%-6d  burst:%d",
                     q,
                     (int)node_queues[n].travelers[q].pid,
                     node_queues[n].travelers[q].burst_time);
            DrawText(entry, panelX + 8, cursor_y, QUEUE_FONT_SIZE - 1,
                     CLR_QUEUE_ENTRY);
            cursor_y += QUEUE_ROW_H;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  draw_entry_flashes  (internal)
 *
 *  Detects WAITING → INSIDE_NODE transitions and draws a brief white
 *  flash ring around the traveler's node to confirm the scheduler fired.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void draw_entry_flashes(const ParentSimulation *sim)
{
    if (!sim) return;

    double now = GetTime();

    for (int i = 0; i < sim->total_travelers && i < MAX_FLASH_SLOTS; i++) {
        const ChildTraveler *t = &sim->travelers[i];

        /* Detect transition: previous state was WAITING, now is INSIDE */
        if (g_prev_state[i] == (int)STATE_WAITING_OUTSIDE &&
            t->visual_state == STATE_INSIDE_NODE) {
            g_flash_start[i] = now;   /* record transition time */
        }
        g_prev_state[i] = (int)t->visual_state;

        /* Is the flash still active? */
        float elapsed = (float)(now - g_flash_start[i]);
        if (elapsed >= FLASH_DURATION || g_flash_start[i] == 0.0) continue;

        /* Fade alpha from 200 → 0 */
        float frac = 1.0f - (elapsed / FLASH_DURATION);
        Color flash = CLR_FLASH;
        flash.a = (unsigned char)(frac * 200.0f);

        /* Draw expanding ring at the traveler's current position */
        float ring_r = 20.0f + (1.0f - frac) * 18.0f;
        DrawCircleLines((int)t->position.x, (int)t->position.y,
                        ring_r, flash);
        DrawCircleLines((int)t->position.x, (int)t->position.y,
                        ring_r + 4.0f, (Color){flash.r, flash.g, flash.b,
                                               (unsigned char)(flash.a / 2)});
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RenderSimulationFrame_M7  (public)
 * ═══════════════════════════════════════════════════════════════════════════ */
void RenderSimulationFrame_M7(const ParentSimulation *sim,
                               const WaitingQueue     *node_queues)
{
    if (!sim) return;

    /*
     * Step 1 – Draw everything from Milestone 6.
     * RenderSimulationFrame() owns its own BeginDrawing()/EndDrawing() pair.
     */
    RenderSimulationFrame(sim);

    /*
     * Step 2 – Overlay the M7 additions in a second BeginDrawing() block.
     * Raylib accumulates draw calls: this effectively layers on top.
     */
    BeginDrawing();

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    /* 2a. Scheduler badge (top-right) */
    draw_scheduler_badge(screenW);

    /* 2b. Waiting queue panel (right side, below badge) */
    draw_queue_panel(sim, node_queues, screenW, screenH);

    /* 2c. Entry-transition flash rings */
    draw_entry_flashes(sim);

    EndDrawing();
}