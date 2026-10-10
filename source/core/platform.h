// platform — the console services the UI needs besides pctl, time and pdm:
// the system number pad, the focus state, hbloader's "next homebrew", the
// console region and random bytes. The Switch build implements them with
// libnx (core/platform.c); the desktop build in the simulator
// (sim/sim_platform.cpp), where PLAYGUARD_SIM_* variables play the console:
//   PLAYGUARD_SIM_NUMPAD=1:30   the number pad returns "1:30" ("cancel": B)
//   PLAYGUARD_SIM_BACKGROUND=1  PlayGuard is not in focus (HOME menu)
//   PLAYGUARD_SIM_HBLOADER=1    started through hbloader: a store can be launched
//   PLAYGUARD_SIM_REGION=2      the console region (SetRegion, 2 = Europe)
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLATFORM_INPUT_OK,          // `out` holds what was typed (not empty)
    PLATFORM_INPUT_CANCELLED,   // B, or nothing typed
    PLATFORM_INPUT_NONE,        // no system keyboard: the caller uses the UI's own
} PlatformInput;

// The system number pad with an extra ':' key ("1:30"), at most `max_len`
// characters, starting from `initial`.
PlatformInput platform_numpad(const char *header, const char *guide, const char *initial, int max_len,
                              char *out, size_t out_size);

// False while the HOME menu or another applet has the screen.
bool platform_in_focus(void);

// PlayGuard was started through hbloader, which can start another homebrew
// once it exits.
bool platform_can_launch(void);
// hbloader starts `path` (an "sdmc:/…" path; also its argv[0]) when PlayGuard
// exits. False when it cannot.
bool platform_set_next_load(const char *path);

// The console region (SetRegion: 0 Japan, 1 Americas, 2 Europe,
// 3 Australia / New Zealand, 4 Hong Kong / Taiwan / Korea, 5 China).
bool platform_region(int *region);

// `size` random bytes; false when none could be made.
bool platform_random(void *buf, size_t size);

// Ends PlayGuard as a crash the console reports (Atmosphère writes a crash
// report with the stack). abort() would not: on the console it returns to
// hbloader as a normal exit, and the next homebrew (hbmenu) is the one that
// crashes, on a PlayGuard thread left running.
void platform_crash(void);

#ifdef __cplusplus
}
#endif
