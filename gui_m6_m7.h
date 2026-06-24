/*
 * gui_m6_m7.h  –  Milestone 7 additions to the GUI layer
 *
 * PURPOSE
 * ───────
 * This header declares the two symbols that Milestone 7 adds on top of
 * gui_m6.h:
 *
 *   1. g_active_sched_name  – a file-scope string set once at startup
 *      (from the CLI argument) so that RenderSimulationFrame_M7() can
 *      display the active scheduling algorithm without coupling main()
 *      to the render path.
 *
 *   2. RenderSimulationFrame_M7()  – a drop-in wrapper around the
 *      existing RenderSimulationFrame() that draws the extra M7 HUD
 *      overlay (scheduler badge + per-node queue indicator) without
 *      modifying any M6 code.
 *
 * HOW TO INTEGRATE
 * ────────────────
 *   • #include "gui_m6_m7.h" AFTER #include "gui_m6.h" in main_m6.c
 *   • Replace every call to RenderSimulationFrame(&sim) in the main
 *     loop with RenderSimulationFrame_M7(&sim).
 *   • Call SetSchedulerDisplayName() once after argv[] is parsed.
 *
 * DO NOT MODIFY gui_m6.h or gui_m6.c – all M7 additions live here.
 */

#ifndef GUI_M6_M7_H
#define GUI_M6_M7_H

#include "gui_m6.h"         /* ParentSimulation, ChildTraveler, etc. */
#include "waiting_queue.h"  /* WaitingQueue                           */

/* ─────────────────────────────────────────────────────────────────────────
 * SetSchedulerDisplayName
 *
 * Call once in main() after the CLI argument for the scheduler is parsed.
 * 'name' must be a string literal or persist for the lifetime of the
 * program (it is stored as a pointer, not copied).
 *
 * Example:
 *   SetSchedulerDisplayName(use_sjf ? "SJF" : "FCFS");
 * ─────────────────────────────────────────────────────────────────────────*/
void SetSchedulerDisplayName(const char *name);

/* ─────────────────────────────────────────────────────────────────────────
 * RenderSimulationFrame_M7
 *
 * Full M7 render pass.  Internally calls RenderSimulationFrame(sim) for
 * all M6 drawing, then overlays:
 *
 *   • Top-right "Scheduler Mode: FCFS/SJF" badge
 *   • Per-node waiting-queue indicator (queue depth + ordered PIDs)
 *   • Smooth transition highlight when a traveler moves from
 *     STATE_WAITING_OUTSIDE → STATE_INSIDE_NODE
 *
 * Parameters:
 *   sim         – master simulation state
 *   node_queues – the per-node WaitingQueue array (length = graph->vertices)
 * ─────────────────────────────────────────────────────────────────────────*/
void RenderSimulationFrame_M7(const ParentSimulation *sim,
                               const WaitingQueue     *node_queues);

#endif /* GUI_M6_M7_H */