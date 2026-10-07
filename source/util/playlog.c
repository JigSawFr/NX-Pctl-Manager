// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "playlog.h"

typedef struct {
    PlayLogTotal *out;
    size_t        count, max;
    uint64_t      now, day_start, week_start;
} Fold;

static uint64_t overlap(uint64_t a, uint64_t b, uint64_t lo, uint64_t hi)
{
    const uint64_t from = a > lo ? a : lo;
    const uint64_t to   = b < hi ? b : hi;
    return to > from ? to - from : 0;
}

// Adds a session that ended at `end_user` and lasted `dur` seconds.
static void add(Fold *f, uint64_t app_id, uint64_t end_user, uint64_t dur)
{
    if (dur == 0 || dur > PLAYLOG_MAX_SESSION_S || end_user < dur) return;
    const uint64_t start = end_user - dur;
    const uint64_t week  = overlap(start, end_user, f->week_start, f->now);
    if (week == 0) return;
    const uint64_t today = overlap(start, end_user, f->day_start, f->now);

    size_t i = 0;
    while (i < f->count && f->out[i].app_id != app_id) i++;
    if (i == f->count) {
        if (f->count == f->max) return;
        f->out[i].app_id  = app_id;
        f->out[i].today_s = 0;
        f->out[i].week_s  = 0;
        f->count++;
    }
    f->out[i].today_s += (uint32_t)today;
    f->out[i].week_s  += (uint32_t)week;
}

// Duration between two events: the steady clock when both have it and it
// went forward, else the user clock; 0 when neither makes sense.
static uint64_t duration(const PlayLogEvent *from, const PlayLogEvent *to)
{
    if (from->ts_steady && to->ts_steady && to->ts_steady >= from->ts_steady)
        return to->ts_steady - from->ts_steady;
    if (to->ts_user >= from->ts_user) return to->ts_user - from->ts_user;
    return 0;
}

size_t playlog_fold(const PlayLogEvent *events, size_t n, uint64_t now,
                    uint64_t day_start, uint64_t week_start,
                    PlayLogTotal *out, size_t max)
{
    Fold f = { out, 0, max, now, day_start, week_start };
    const PlayLogEvent *open = NULL;   // the focus event of the game being played

    for (size_t i = 0; i < n; i++) {
        const PlayLogEvent *e = &events[i];
        switch (e->kind) {
            case PlayLogEv_Focus:
                if (open && open->app_id == e->app_id) break;   // repeated: keep the first
                if (open) add(&f, open->app_id, e->ts_user, duration(open, e));
                open = e->app_id ? e : NULL;
                break;
            case PlayLogEv_Unfocus:
                if (!open || open->app_id != e->app_id) break;   // not the game in focus
                add(&f, open->app_id, e->ts_user, duration(open, e));
                open = NULL;
                break;
            case PlayLogEv_Away:
                if (open) add(&f, open->app_id, e->ts_user, duration(open, e));
                open = NULL;
                break;
            default:
                break;
        }
    }
    // Still in focus (PlayGuard started over that game, for instance).
    if (open && now >= open->ts_user) add(&f, open->app_id, now, now - open->ts_user);
    return f.count;
}
