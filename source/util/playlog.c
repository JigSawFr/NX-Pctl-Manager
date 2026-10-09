// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "playlog.h"

typedef struct {
    PlayLogTotal *out;
    size_t        count, max;
    uint64_t      now;
    uint64_t      day_starts[7];
    const PlayLogSpan *skip;
    size_t        n_skip;
} Fold;

static uint64_t overlap(uint64_t a, uint64_t b, uint64_t lo, uint64_t hi)
{
    const uint64_t from = a > lo ? a : lo;
    const uint64_t to   = b < hi ? b : hi;
    return to > from ? to - from : 0;
}

// Adds the play time in [start, end_user).
static void add_range(Fold *f, uint64_t app_id, uint64_t start, uint64_t end_user)
{
    const uint64_t week  = overlap(start, end_user, f->day_starts[6], f->now);
    if (week == 0) return;

    size_t i = 0;
    while (i < f->count && f->out[i].app_id != app_id) i++;
    if (i == f->count) {
        if (f->count == f->max) return;
        PlayLogTotal *t = &f->out[i];
        t->app_id = app_id;
        t->today_s = t->week_s = 0;
        for (int k = 0; k < 7; k++) t->day_s[k] = 0;
        f->count++;
    }
    PlayLogTotal *t = &f->out[i];
    t->week_s += (uint32_t)week;
    for (int k = 0; k < 7; k++) {   // a session across midnight counts on both days
        const uint64_t end = k == 0 ? f->now : f->day_starts[k - 1];
        t->day_s[k] += (uint32_t)overlap(start, end_user, f->day_starts[k], end);
    }
    t->today_s = t->day_s[0];
}

// [start, end) without the skipped spans: the first piece, recursively the rest.
static void add_cut(Fold *f, uint64_t app_id, uint64_t start, uint64_t end, size_t from)
{
    for (size_t i = from; i < f->n_skip; i++) {
        const PlayLogSpan *s = &f->skip[i];
        if (s->end <= start || s->start >= end) continue;   // no overlap
        if (s->start > start) add_cut(f, app_id, start, s->start, i + 1);
        if (s->end < end) add_cut(f, app_id, s->end, end, i + 1);
        return;
    }
    if (end > start) add_range(f, app_id, start, end);
}

// Adds a session that ended at `end_user` and lasted `dur` seconds.
static void add(Fold *f, uint64_t app_id, uint64_t end_user, uint64_t dur)
{
    if (dur == 0 || dur > PLAYLOG_MAX_SESSION_S || end_user < dur) return;
    add_cut(f, app_id, end_user - dur, end_user, 0);
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
    // Whole days between the two (the old interface knew no others).
    uint64_t starts[7];
    starts[0] = day_start;
    for (int k = 1; k < 7; k++) {
        const uint64_t back = (uint64_t)k * 86400u;
        starts[k] = day_start > week_start + back ? day_start - back : week_start;
    }
    starts[6] = week_start;
    return playlog_fold_days(events, n, now, starts, out, max);
}

size_t playlog_fold_days(const PlayLogEvent *events, size_t n, uint64_t now,
                         const uint64_t day_starts[7], PlayLogTotal *out, size_t max)
{
    return playlog_fold_days_skip(events, n, now, day_starts, NULL, 0, out, max);
}

size_t playlog_fold_days_skip(const PlayLogEvent *events, size_t n, uint64_t now,
                              const uint64_t day_starts[7], const PlayLogSpan *skip, size_t n_skip,
                              PlayLogTotal *out, size_t max)
{
    Fold f;
    f.skip = skip;
    f.n_skip = skip ? n_skip : 0;
    f.out = out;
    f.count = 0;
    f.max = max;
    f.now = now;
    for (int k = 0; k < 7; k++) f.day_starts[k] = day_starts[k];
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
            case PlayLogEv_Launch:
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

// An event of `kind` for `app_id` at the time of `at`.
static void emit(PlayLogEvent *out, size_t *count, size_t max, uint8_t kind, uint64_t app_id, const PlayLogEvent *at)
{
    if (*count == max) return;
    PlayLogEvent *e = &out[(*count)++];
    e->app_id    = app_id;
    e->kind      = kind;
    e->ts_user   = at->ts_user;
    e->ts_steady = at->ts_steady;
    e->uid[0] = e->uid[1] = 0;
}

size_t playlog_for_account(const PlayLogEvent *events, size_t n, const uint64_t uid[2],
                           PlayLogEvent *out, size_t max)
{
    size_t count = 0;
    uint64_t focused = 0;       // the game with the focus, 0 for none
    bool     open    = false;   // `uid` is open in a game
    bool     playing = false;   // a Focus was emitted and not closed yet

    for (size_t i = 0; i < n; i++) {
        const PlayLogEvent *e = &events[i];
        const bool mine = e->uid[0] == uid[0] && e->uid[1] == uid[1];
        switch (e->kind) {
            case PlayLogEv_Focus:
                if (focused == e->app_id && playing) break;   // repeated
                if (playing) emit(out, &count, max, PlayLogEv_Away, 0, e);
                focused = e->app_id;
                playing = open && focused;
                if (playing) emit(out, &count, max, PlayLogEv_Focus, focused, e);
                break;
            case PlayLogEv_Unfocus:
                if (focused != e->app_id) break;   // not the game in focus
                if (playing) emit(out, &count, max, PlayLogEv_Unfocus, focused, e);
                focused = 0;
                playing = false;
                break;
            case PlayLogEv_Away:
            case PlayLogEv_Launch:
                if (playing) emit(out, &count, max, PlayLogEv_Away, 0, e);
                focused = 0;
                playing = false;
                if (e->kind == PlayLogEv_Launch) open = false;   // the previous game's accounts
                break;
            case PlayLogEv_AccountOpen:
                if (!mine) break;
                open = true;
                // Selected while the game already has the focus (the user
                // picker comes after the launch): it counts from now.
                if (focused && !playing) {
                    emit(out, &count, max, PlayLogEv_Focus, focused, e);
                    playing = true;
                }
                break;
            case PlayLogEv_AccountClose:
                if (!mine) break;
                open = false;
                if (playing) emit(out, &count, max, PlayLogEv_Unfocus, focused, e);
                playing = false;
                break;
            default:
                break;
        }
    }
    return count;
}
