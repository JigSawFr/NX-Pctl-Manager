// Host tests for util/activity_summary.hpp: the Activity tab's summary over
// each period (average per day, most played game, days played, busiest day,
// average session), what is left out when a figure was not read, and the
// games to rediscover.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

#include "util/activity_summary.hpp"

using namespace activity_summary;

static const uint64_t DAY = 86400, NOW = 1760000000;

static GameStat& add(PlayStats& s, const char* name, uint64_t total_min, uint32_t launches, uint64_t last_days_ago)
{
    GameStat& g = s.games[s.count++];
    g.app_id = 0x0100000000010000ULL + ((uint64_t)s.count << 12);
    std::snprintf(g.name, sizeof g.name, "%s", name);
    g.totals_ok = true;
    g.total_s = total_min * 60;
    g.launches = launches;
    g.last_played = NOW - last_days_ago * DAY;
    g.first_played = NOW - 99 * DAY - 3600;   // 99 whole days and an hour ago: day 100
    return g;
}

static std::unique_ptr<PlayStats> sample()
{
    auto s = std::make_unique<PlayStats>();
    std::memset(s.get(), 0, sizeof *s);
    s->windows_ok = true;
    s->now = NOW;
    for (int k = 0; k < 7; k++) s->day_wday[k] = (u8)((3 - k + 7) % 7);
    GameStat& a = add(*s, "Kart", 600, 20, 0);   // 10 h, this week 70 min (today 30)
    a.today_s = a.day_s[0] = 30 * 60;
    a.day_s[2] = 40 * 60;
    a.week_s = 70 * 60;
    GameStat& b = add(*s, "Farm", 400, 20, 2);   // this week 80 min, all on day 2
    b.day_s[2] = b.week_s = 80 * 60;
    return s;
}

static void test_periods()
{
    auto s = sample();
    Summary t = summarize(*s, Today);
    assert(t.known && t.total_s == 30 * 60 && t.top == 0 && t.top_s == 30 * 60);
    assert(t.days == 0 && t.days_played == -1 && t.busiest == -1 && t.session_s == 0);

    Summary w = summarize(*s, Week);
    assert(w.known && w.total_s == 150 * 60);
    assert(w.days == 7 && w.per_day_s == 150 * 60 / 7);
    assert(w.top == 1 && w.top_s == 80 * 60);            // Farm, 80 min
    assert(w.days_played == 2 && w.busiest == 2 && w.busiest_s == 120 * 60);
    assert(w.session_s == 0);

    Summary all = summarize(*s, AllTime);
    assert(all.known && all.total_s == 1000 * 60 && all.top == 0);
    assert(all.days == 100 && all.per_day_s == 600);      // 1000 min over 100 days
    assert(all.session_s == 1000 * 60 / 40);              // 25 min a launch
    assert(all.days_played == -1 && all.busiest == -1);
}

static void test_edges()
{
    // A tie: the one played last first, as the list.
    auto s = sample();
    s->games[0].last_played = NOW - DAY;
    s->games[1].week_s = 70 * 60;
    s->games[1].last_played = NOW - 3600;
    assert(summarize(*s, Week).top == 1);
    s->games[1].last_played = NOW - 5 * DAY;
    assert(summarize(*s, Week).top == 0);

    // Nothing played today: no top game.
    s = sample();
    s->games[0].today_s = 0;
    Summary t = summarize(*s, Today);
    assert(t.known && t.total_s == 0 && t.top == -1);

    // The log unreadable: today and the week unknown, all time still known.
    s = sample();
    s->windows_ok = false;
    assert(!summarize(*s, Today).known && !summarize(*s, Week).known && summarize(*s, AllTime).known);
    // The statistics unreadable: all time unknown.
    s = sample();
    s->stats_rc = 0x1A0C;
    assert(!summarize(*s, AllTime).known && summarize(*s, Week).known);
    // The game list unreadable: nothing.
    s = sample();
    s->rc = 0x1A0C;
    for (int p = 0; p < 3; p++) assert(!summarize(*s, p).known);

    // A game without totals counts for none of them, and no first play known
    // means no average per day.
    s = sample();
    s->games[1].totals_ok = false;
    for (uint32_t i = 0; i < s->count; i++) s->games[i].first_played = 0;
    Summary all = summarize(*s, AllTime);
    assert(all.total_s == 600 * 60 && all.days == 0 && all.per_day_s == 0 && all.session_s == 600 * 60 / 20);
    // No launch counted: no session length.
    s->games[0].launches = 0;
    assert(summarize(*s, AllTime).session_s == 0);

    // A first play "in the future" (the clock went back): one day.
    s = sample();
    s->games[0].first_played = s->games[1].first_played = NOW + DAY;
    assert(summarize(*s, AllTime).days == 1);
    // First played earlier today: one day.
    s->games[0].first_played = s->games[1].first_played = NOW - 60;
    assert(summarize(*s, AllTime).days == 1);

    // Nothing played this week: no busiest day.
    s = sample();
    for (uint32_t i = 0; i < s->count; i++) {
        s->games[i].week_s = s->games[i].today_s = 0;
        std::memset(s->games[i].day_s, 0, sizeof s->games[i].day_s);
    }
    Summary w = summarize(*s, Week);
    assert(w.days_played == 0 && w.busiest == -1 && w.per_day_s == 0 && w.top == -1);
}

static void test_rediscover()
{
    auto s = sample();
    assert(rediscover(*s, 3).empty());   // both played recently, and long

    add(*s, "Puzzle", 45, 2, 60);        // 45 min, 60 days ago
    add(*s, "Tennis", 45, 3, 90);        // as little, left aside longer
    add(*s, "Quiz", 20, 1, 30);          // exactly 30 days: in
    add(*s, "Recent", 10, 1, 29);        // 29 days: out
    add(*s, "Long", 180, 9, 200);        // 3 h: out
    add(*s, "", 5, 1, 200);              // deleted (no name): out
    GameStat& unread = add(*s, "Unread", 5, 1, 200);
    unread.totals_ok = false;            // its totals unknown: out
    GameStat& never = add(*s, "Never", 5, 1, 200);
    never.last_played = 0;               // last play unknown: out
    GameStat& week = add(*s, "Week", 5, 1, 200);
    week.week_s = 60;                    // the log has it this week: out
    GameStat& future = add(*s, "Future", 5, 1, 0);
    future.last_played = NOW + DAY;      // the clock went back: out

    std::vector<int> r = rediscover(*s, 3);
    assert(r.size() == 3);
    assert(!std::strcmp(s->games[r[0]].name, "Quiz"));
    assert(!std::strcmp(s->games[r[1]].name, "Tennis"));
    assert(!std::strcmp(s->games[r[2]].name, "Puzzle"));
    assert(rediscover(*s, 1).size() == 1 && rediscover(*s, 10).size() == 3);

    // Without the log, the "this week" check is left out.
    s->windows_ok = false;
    assert(rediscover(*s, 10).size() == 4);
    // Without the statistics, nothing.
    s->stats_rc = 0x1A0C;
    assert(rediscover(*s, 10).empty());
}

int main()
{
    test_periods();
    test_edges();
    test_rediscover();
    std::puts("activity_summary assertions passed");
    return 0;
}
