// Host tests for source/util/playlog.c: folding the console's play-event log
// into play time per game for today and the last 7 days.
#include "check.h"
#include <stdio.h>

#include "playlog.h"

enum { A = 1, B = 2, C = 3 };
enum { H = 3600, DAY = 86400 };

// "Now" is 18:00 on day 10; today started at midnight; the week six days earlier.
static const uint64_t DAY_START  = 10 * DAY;
static const uint64_t NOW        = 10 * DAY + 18 * H;
static const uint64_t WEEK_START = 4 * DAY;

static PlayLogEvent ev(uint64_t app, uint8_t kind, uint64_t user)
{
    // Steady clock: the user clock with an arbitrary offset (no clock change).
    PlayLogEvent e = { app, kind, user, user + 5000, { 0, 0 } };
    return e;
}

static size_t fold(const PlayLogEvent *e, size_t n, PlayLogTotal *out)
{
    return playlog_fold(e, n, NOW, DAY_START, WEEK_START, out, 8);
}

static const PlayLogTotal *find(const PlayLogTotal *t, size_t n, uint64_t app)
{
    for (size_t i = 0; i < n; i++)
        if (t[i].app_id == app) return &t[i];
    return NULL;
}

static void test_sessions(void)
{
    PlayLogTotal t[8];
    // Today 9:00-10:00 A; yesterday 20:00-21:30 B; then A again 11:00-11:30.
    const PlayLogEvent log[] = {
        ev(B, PlayLogEv_Focus, 9 * DAY + 20 * H), ev(B, PlayLogEv_Unfocus, 9 * DAY + 21 * H + 1800),
        ev(A, PlayLogEv_Focus, DAY_START + 9 * H),  ev(A, PlayLogEv_Unfocus, DAY_START + 10 * H),
        ev(A, PlayLogEv_Focus, DAY_START + 11 * H), ev(A, PlayLogEv_Unfocus, DAY_START + 11 * H + 1800),
    };
    size_t n = fold(log, 6, t);
    CHECK(n == 2);
    CHECK(find(t, n, A)->today_s == H + 1800 && find(t, n, A)->week_s == H + 1800);
    CHECK(find(t, n, B)->today_s == 0 && find(t, n, B)->week_s == H + 1800);
}

static void test_windows(void)
{
    PlayLogTotal t[8];
    // 23:00 yesterday to 01:00 today: one hour on each side of midnight.
    const PlayLogEvent midnight[] = {
        ev(A, PlayLogEv_Focus, DAY_START - H), ev(A, PlayLogEv_Unfocus, DAY_START + H),
    };
    size_t n = fold(midnight, 2, t);
    CHECK(n == 1 && t[0].today_s == H && t[0].week_s == 2 * H);

    // Across the start of the week window, and entirely before it.
    const PlayLogEvent week[] = {
        ev(A, PlayLogEv_Focus, WEEK_START - 2 * H), ev(A, PlayLogEv_Unfocus, WEEK_START + H),
        ev(B, PlayLogEv_Focus, WEEK_START - 5 * H), ev(B, PlayLogEv_Unfocus, WEEK_START - 4 * H),
    };
    n = fold(week, 4, t);
    CHECK(n == 1 && t[0].app_id == A && t[0].week_s == H && t[0].today_s == 0);
}

static void test_cut_short(void)
{
    PlayLogTotal t[8];
    // A crashes: no "out of focus", the HOME menu gets the focus at 10:00.
    // B is cut short by sleep at 14:00. C gets the focus at 15:00 and is
    // still being played now (18:00).
    const PlayLogEvent log[] = {
        ev(A, PlayLogEv_Focus, DAY_START + 9 * H),
        ev(0, PlayLogEv_Away,  DAY_START + 10 * H),
        ev(A, PlayLogEv_Unfocus, DAY_START + 12 * H),   // stray: A is not in focus
        ev(B, PlayLogEv_Focus, DAY_START + 13 * H),
        ev(B, PlayLogEv_Focus, DAY_START + 13 * H + 600), // repeated: the first one counts
        ev(0, PlayLogEv_Away,  DAY_START + 14 * H),
        ev(C, PlayLogEv_Focus, DAY_START + 15 * H),
    };
    size_t n = fold(log, 7, t);
    CHECK(n == 3);
    CHECK(find(t, n, A)->today_s == H);
    CHECK(find(t, n, B)->today_s == H);
    CHECK(find(t, n, C)->today_s == 3 * H);

    // Another game getting the focus closes the open one.
    const PlayLogEvent swap[] = {
        ev(A, PlayLogEv_Focus, DAY_START + 9 * H), ev(B, PlayLogEv_Focus, DAY_START + 9 * H + 900),
        ev(B, PlayLogEv_Unfocus, DAY_START + 10 * H),
    };
    n = fold(swap, 3, t);
    CHECK(n == 2 && find(t, n, A)->today_s == 900 && find(t, n, B)->today_s == 2700);
}

static void test_clock_changes(void)
{
    PlayLogTotal t[8];
    // The user clock jumps 2 h forward during the session: the steady clock
    // says 30 min, and the session is placed before its end on the user clock.
    PlayLogEvent jump[] = {
        { A, PlayLogEv_Focus,   DAY_START + 9 * H,              1000, { 0, 0 } },
        { A, PlayLogEv_Unfocus, DAY_START + 9 * H + 2 * H + 1800, 1000 + 1800, { 0, 0 } },
    };
    size_t n = fold(jump, 2, t);
    CHECK(n == 1 && t[0].today_s == 1800);

    // No steady clock: the user clock is used; backwards on both: dropped.
    PlayLogEvent no_steady[] = {
        { A, PlayLogEv_Focus,   DAY_START + 9 * H,  0, { 0, 0 } },
        { A, PlayLogEv_Unfocus, DAY_START + 10 * H, 0, { 0, 0 } },
        { B, PlayLogEv_Focus,   DAY_START + 12 * H, 900, { 0, 0 } },
        { B, PlayLogEv_Unfocus, DAY_START + 11 * H, 800, { 0, 0 } },
    };
    n = fold(no_steady, 4, t);
    CHECK(n == 1 && t[0].app_id == A && t[0].today_s == H);

    // A "session" of more than 24 h is a broken log, not play time.
    const PlayLogEvent broken[] = {
        ev(A, PlayLogEv_Focus, WEEK_START + H), ev(A, PlayLogEv_Unfocus, WEEK_START + 2 * DAY),
    };
    CHECK(fold(broken, 2, t) == 0);

    // Events after "now" (clock moved back since) are clipped to now.
    const PlayLogEvent future[] = {
        ev(A, PlayLogEv_Focus, NOW - H), ev(A, PlayLogEv_Unfocus, NOW + H),
    };
    n = fold(future, 2, t);
    CHECK(n == 1 && t[0].today_s == H);
}

static void test_limits(void)
{
    PlayLogTotal t[2];
    const PlayLogEvent log[] = {
        ev(A, PlayLogEv_Focus, DAY_START + H), ev(A, PlayLogEv_Unfocus, DAY_START + 2 * H),
        ev(B, PlayLogEv_Focus, DAY_START + 3 * H), ev(B, PlayLogEv_Unfocus, DAY_START + 4 * H),
        ev(C, PlayLogEv_Focus, DAY_START + 5 * H), ev(C, PlayLogEv_Unfocus, DAY_START + 6 * H),
        ev(A, PlayLogEv_Focus, DAY_START + 7 * H), ev(A, PlayLogEv_Unfocus, DAY_START + 8 * H),
    };
    size_t n = playlog_fold(log, 8, NOW, DAY_START, WEEK_START, t, 2);
    CHECK(n == 2 && find(t, n, C) == NULL);   // no room for a third game
    CHECK(find(t, n, A)->today_s == 2 * H);    // known games still add up
    CHECK(playlog_fold(NULL, 0, NOW, DAY_START, WEEK_START, t, 2) == 0);
}

static void test_days(void)
{
    PlayLogTotal t[8];
    // Through playlog_fold: whole days back from today's midnight.
    const PlayLogEvent log[] = {
        ev(A, PlayLogEv_Focus, 9 * DAY + 23 * H),   ev(A, PlayLogEv_Unfocus, DAY_START + H),        // across midnight
        ev(A, PlayLogEv_Focus, 6 * DAY + 10 * H),   ev(A, PlayLogEv_Unfocus, 6 * DAY + 12 * H),     // 4 days back
        ev(B, PlayLogEv_Focus, WEEK_START + 2 * H), ev(B, PlayLogEv_Unfocus, WEEK_START + 3 * H),   // 6 days back
    };
    size_t n = fold(log, 6, t);
    const PlayLogTotal *a = find(t, n, A), *b = find(t, n, B);
    CHECK(a->day_s[0] == H && a->day_s[1] == H && a->day_s[4] == 2 * H);
    CHECK(a->day_s[2] == 0 && a->day_s[3] == 0 && a->day_s[5] == 0 && a->day_s[6] == 0);
    CHECK(a->today_s == a->day_s[0] && a->week_s == 4 * H);
    CHECK(b->day_s[6] == H && b->week_s == H && b->today_s == 0);

    // Days as the calendar gives them: yesterday was 23 h long (the clocks
    // went forward), so the day before started 23 h before yesterday's start.
    uint64_t starts[7] = { DAY_START, DAY_START - 23 * H };
    for (int k = 2; k < 7; k++) starts[k] = starts[k - 1] - DAY;
    const PlayLogEvent around[] = {
        ev(C, PlayLogEv_Focus, starts[1] - H), ev(C, PlayLogEv_Unfocus, starts[1] + H),
    };
    n = playlog_fold_days(around, 2, NOW, starts, t, 8);
    CHECK(n == 1 && t[0].day_s[2] == H && t[0].day_s[1] == H && t[0].week_s == 2 * H);
    uint32_t sum = 0;
    for (int k = 0; k < 7; k++) sum += t[0].day_s[k];
    CHECK(sum == t[0].week_s);
}

static PlayLogEvent acc(uint8_t kind, uint64_t user, uint64_t who)
{
    PlayLogEvent e = { 0, kind, user, user + 5000, { who, who + 100 } };
    return e;
}

static void test_accounts(void)
{
    enum { ALICE = 7, LEO = 9 };
    const uint64_t alice[2] = { ALICE, ALICE + 100 }, leo[2] = { LEO, LEO + 100 };
    const uint64_t T = DAY_START + 9 * H;
    // A starts at 9:00, Alice is picked at 9:05; HOME at 9:30 (the account
    // stays open), back at 9:40; Leo joins at 9:50 (local play); A exits at
    // 10:00 and closes both. Then B launches with Leo only, 11:00-11:30.
    const PlayLogEvent log[] = {
        ev(A, PlayLogEv_Launch, T),            ev(A, PlayLogEv_Focus, T),
        acc(PlayLogEv_AccountOpen, T + 300, ALICE),
        ev(0, PlayLogEv_Away, T + 1800),       ev(A, PlayLogEv_Focus, T + 2400),
        acc(PlayLogEv_AccountOpen, T + 3000, LEO),
        ev(A, PlayLogEv_Unfocus, T + H),
        acc(PlayLogEv_AccountClose, T + H, ALICE), acc(PlayLogEv_AccountClose, T + H, LEO),
        ev(B, PlayLogEv_Launch, T + 2 * H),    ev(B, PlayLogEv_Focus, T + 2 * H),
        acc(PlayLogEv_AccountOpen, T + 2 * H, LEO),
        ev(B, PlayLogEv_Unfocus, T + 2 * H + 1800),
    };
    const size_t n = sizeof(log) / sizeof(log[0]);
    PlayLogEvent mine[32];
    PlayLogTotal t[8];

    size_t m = playlog_for_account(log, n, alice, mine, 32);
    size_t k = fold(mine, m, t);
    CHECK(k == 1 && find(t, k, A)->today_s == 1500 + 1200);   // 9:05-9:30 and 9:40-10:00

    m = playlog_for_account(log, n, leo, mine, 32);
    k = fold(mine, m, t);
    CHECK(k == 2 && find(t, k, A)->today_s == 600 && find(t, k, B)->today_s == 1800);

    // Everyone: the whole focus time, account events ignored.
    k = fold(log, n, t);
    CHECK(find(t, k, A)->today_s == 1800 + 1200 && find(t, k, B)->today_s == 1800);

    // A game that crashed without closing its account: the next launch does.
    const PlayLogEvent crash[] = {
        ev(A, PlayLogEv_Focus, T), acc(PlayLogEv_AccountOpen, T, ALICE), ev(0, PlayLogEv_Away, T + 600),
        ev(B, PlayLogEv_Launch, T + H), ev(B, PlayLogEv_Focus, T + H), ev(B, PlayLogEv_Unfocus, T + 2 * H),
    };
    m = playlog_for_account(crash, 6, alice, mine, 32);
    k = fold(mine, m, t);
    CHECK(k == 1 && find(t, k, A)->today_s == 600 && find(t, k, B) == NULL);

    // Nothing for an account that never played; the output never overflows.
    const uint64_t nobody[2] = { 1, 2 };
    CHECK(playlog_for_account(log, n, nobody, mine, 32) == 0);
    CHECK(playlog_for_account(log, n, leo, mine, 1) == 1);
}

int main(void)
{
    test_sessions();
    test_windows();
    test_cut_short();
    test_clock_changes();
    test_limits();
    test_days();
    test_accounts();
    return CHECK_DONE("playlog session, window and per-account assertions passed");
}
