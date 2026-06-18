#ifndef GUI_PARENT_H
#define GUI_PARENT_H

#include <sys/types.h>
#include <unistd.h>
#include "graph.h"
#include "raylib.h"
#include "gui_m6.h"

// שימוש בקבועים המקוריים של gui_m6 כדי למנוע redefinition
#undef LOG_EVENT_ARRIVED
#undef LOG_EVENT_DESTINATION
#undef LOG_EVENT_FINISHED
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT

#define LOG_EVENT_ARRIVED      0
#define LOG_EVENT_DESTINATION  1
#define LOG_EVENT_FINISHED     2

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
#define TARGET_FPS    60

void ParseExtendedInputFiles(ParentSimulation *sim, const char *matrix_file, const char *travelers_file);
void InitializeRaylibVisualEnvironment(ParentSimulation *sim);
void ImplementNonBlockingIPCMonitoring(ParentSimulation *sim);
void RenderMultiColorTravelerIndicators(const ParentSimulation *sim); // הוספת const תואם
void ExecuteCentralLoggingOutput(const ChildTraveler *updater, int event_type, int current_node);
void SynchronizeProcessTerminations(ParentSimulation *sim);
void FreeParentSimulation(ParentSimulation *sim);

#endif
