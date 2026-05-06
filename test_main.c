/*
 * test_main.c  –  Headless entry-point for CI / Valgrind runs.
 *
 * Compile with -DHEADLESS so no Raylib window is opened.
 * Calls RunSystemTests() then exits cleanly.
 */

#include <stdio.h>
#include "gui.h"

int main(void)
{
    RunSystemTests();
    printf("All tests completed. Exiting cleanly.\n");
    return 0;
}
