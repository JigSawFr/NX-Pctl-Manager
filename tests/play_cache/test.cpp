// Host tests for util/play_cache.hpp: the cached play data's file and the
// shift of its day windows.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

#include "util/play_cache.hpp"

int main()
{
    auto s = std::make_unique<PlayStats>();
    std::memset(s.get(), 0, sizeof *s);
    s->windows_ok = true;
    s->now = 1760000000;
    for (int k = 0; k < 7; k++) s->day_wday[k] = (u8)((3 - k + 7) % 7);   // today: Wednesday
    s->count = 2;
    std::strcpy(s->games[0].name, "Game A");
    s->games[0].app_id = 0x0100000000010000ULL;
    s->games[0].totals_ok = true;
    s->games[0].total_s = 99999;
    for (int k = 0; k < 7; k++) s->games[0].day_s[k] = 100 * (k + 1);   // 100 today … 700 six days ago
    s->games[0].today_s = 100;
    s->games[0].week_s = 2800;
    s->games[1].app_id = 0x0100000000020000ULL;
    s->games[1].day_s[6] = 60;
    s->games[1].week_s = 60;

    // Round trip; only the games held are written.
    const std::string bytes = play_cache::encode(*s);
    assert(bytes.size() < sizeof(PlayStats) / 10);
    auto back = std::make_unique<PlayStats>();
    assert(play_cache::decode(bytes, *back));
    assert(back->count == 2 && back->now == s->now && back->windows_ok);
    assert(std::strcmp(back->games[0].name, "Game A") == 0 && back->games[0].total_s == 99999);
    assert(back->games[1].day_s[6] == 60);

    // A broken or foreign file is refused.
    assert(!play_cache::decode("", *back));
    assert(!play_cache::decode(bytes.substr(0, bytes.size() - 1), *back));
    std::string other = bytes;
    other[0] = 'X';
    assert(!play_cache::decode(other, *back));
    other = bytes;
    other[4] = 9;   // version
    assert(!play_cache::decode(other, *back));

    // Two days later: today and yesterday empty, the rest moved back.
    assert(play_cache::decode(bytes, *back));
    play_cache::shift_days(*back, 2);
    const GameStat& a = back->games[0];
    assert(a.day_s[0] == 0 && a.day_s[1] == 0 && a.day_s[2] == 100 && a.day_s[6] == 500);
    assert(a.today_s == 0 && a.week_s == 100 + 200 + 300 + 400 + 500);
    assert(back->games[1].week_s == 0);
    assert(back->day_wday[0] == 5 && back->day_wday[2] == 3 && back->day_wday[6] == 6);   // Friday first
    assert(a.total_s == 99999);   // all time untouched

    // A week or more: nothing in the windows. The same day: unchanged.
    assert(play_cache::decode(bytes, *back));
    play_cache::shift_days(*back, 9);
    assert(back->windows_ok && back->games[0].week_s == 0 && back->games[0].today_s == 0);
    assert(play_cache::decode(bytes, *back));
    play_cache::shift_days(*back, 0);
    assert(back->games[0].week_s == 2800);

    // The clock went back: the windows are not shown.
    play_cache::shift_days(*back, -1);
    assert(!back->windows_ok);
    puts("play_cache assertions passed");
    return 0;
}
