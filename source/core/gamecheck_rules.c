// PlayGuard — what a game's facts mean (see gamecheck.h). No console calls:
// compiled for the Switch, the desktop simulator and the host tests.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "gamecheck.h"

u32 gamecheck_sysver_to_hos(u32 v)
{
    if (!v) return 0;
    return MAKEHOSVERSION((v >> 26) & 0x3F, (v >> 20) & 0x3F, (v >> 16) & 0xF);
}

GameIssue gamecheck_classify(const GameFacts *g, u32 current_hos)
{
    if (g->meta_count == 0) return GC_ARCHIVED;
    if (g->card_only) return GC_OK;   // the card is not inserted, or reads fine
    if (g->storage_unavailable) return GC_STORAGE_UNAVAILABLE;
    if (!g->has_base) return GC_NO_BASE;
    if (g->meta_missing || g->files_missing) return GC_FILES_MISSING;
    if (g->required_hos && current_hos && g->required_hos > current_hos) return GC_FIRMWARE_TOO_OLD;
    if (R_FAILED(g->launch_rc)) return GC_LAUNCH_REFUSED;
    if (!g->control_ok) return GC_NO_CONTROL;
    return GC_OK;
}

bool gamecheck_removable(GameIssue issue)
{
    return issue != GC_OK && issue != GC_STORAGE_UNAVAILABLE && issue < GC_ISSUE_COUNT;
}
