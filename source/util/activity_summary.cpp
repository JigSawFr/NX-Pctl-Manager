// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/activity_summary.hpp"

#include <algorithm>

namespace activity_summary
{

uint64_t value(const GameStat& g, int period)
{
    switch (period) {
        case Today: return g.today_s;
        case Week:  return g.week_s;
        default:    return g.totals_ok ? g.total_s : 0;
    }
}

Summary summarize(const PlayStats& s, int period)
{
    Summary out;
    out.known = period == AllTime ? R_SUCCEEDED(s.stats_rc) : s.windows_ok;
    if (R_FAILED(s.rc) || !out.known) {
        out.known = false;
        return out;
    }
    uint64_t launches = 0, first = 0;
    uint64_t per_day[7] = {};
    for (uint32_t i = 0; i < s.count && i < PLAYSTATS_MAX; i++) {
        const GameStat& g = s.games[i];
        const uint64_t v = value(g, period);
        out.total_s += v;
        // The most played, the latest played first on a tie (as the list).
        if (v > 0 && (out.top < 0 || v > out.top_s || (v == out.top_s && g.last_played > s.games[out.top].last_played))) {
            out.top = (int)i;
            out.top_s = v;
        }
        if (g.totals_ok) {
            launches += g.launches;
            if (g.first_played && (!first || g.first_played < first)) first = g.first_played;
        }
        for (int k = 0; k < 7; k++) per_day[k] += g.day_s[k];
    }
    if (period == Week) {
        out.days = 7;
        out.per_day_s = out.total_s / 7;
        out.days_played = 0;
        for (int k = 0; k < 7; k++) {
            if (per_day[k]) out.days_played++;
            if (per_day[k] > out.busiest_s) {   // the most recent day on a tie
                out.busiest = k;
                out.busiest_s = per_day[k];
            }
        }
    } else if (period == AllTime) {
        if (first) {
            // Whole days since the first play, today included; a first play
            // "in the future" (the clock went back) counts as today.
            out.days = s.now > first ? (int)((s.now - first) / 86400) + 1 : 1;
            out.per_day_s = out.total_s / (uint64_t)out.days;
        }
        if (launches) out.session_s = out.total_s / launches;
    }
    return out;
}

std::vector<int> rediscover(const PlayStats& s, size_t max)
{
    std::vector<int> out;
    if (R_FAILED(s.rc) || R_FAILED(s.stats_rc)) return out;
    for (uint32_t i = 0; i < s.count && i < PLAYSTATS_MAX; i++) {
        const GameStat& g = s.games[i];
        if (!g.name[0] || !g.totals_ok || !g.last_played || g.total_s >= REDISCOVER_MAX_S) continue;
        if (s.now < g.last_played || s.now - g.last_played < REDISCOVER_AFTER_S) continue;
        if (s.windows_ok && g.week_s) continue;   // the log says otherwise
        out.push_back((int)i);
    }
    // The least played first, then the one left aside the longest.
    std::sort(out.begin(), out.end(), [&s](int a, int b) {
        const GameStat& ga = s.games[a];
        const GameStat& gb = s.games[b];
        return ga.total_s != gb.total_s ? ga.total_s < gb.total_s : ga.last_played < gb.last_played;
    });
    if (out.size() > max) out.resize(max);
    return out;
}

}   // namespace activity_summary
