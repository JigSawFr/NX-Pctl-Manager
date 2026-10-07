// Host tests for source/util/duration.cpp: the forms a daily limit can be typed in.
#include <cassert>
#include <cstdio>

#include "util/duration.hpp"

static int parsed(const char* s)
{
    uint16_t m = 0xFFFF;
    return duration::parse(s, &m) ? m : -1;
}

int main()
{
    assert(parsed("90") == 90);
    assert(parsed("0") == 0);
    assert(parsed("1440") == 1440);
    assert(parsed(" 45 ") == 45);
    assert(parsed("1:30") == 90);
    assert(parsed("0:45") == 45);
    assert(parsed("1:05") == 65);
    assert(parsed("24:00") == 1440);
    assert(parsed("2h") == 120);
    assert(parsed("2H30") == 150);
    assert(parsed("1 h 30") == 90);
    assert(parsed("1h30m") == 90);
    assert(parsed("90m") == 90);
    assert(parsed("90 min") == 90);
    assert(parsed("90 MIN") == 90);
    assert(parsed("45mn") == 45);
    assert(parsed(" 90 m ") == 90);
    assert(parsed("1h30min") == 90);
    assert(parsed("1 h 30 min") == 90);
    assert(parsed("2 h") == 120);

    assert(parsed("") == -1);
    assert(parsed(":30") == -1);
    assert(parsed("1:") == -1);
    assert(parsed("1:60") == -1);
    assert(parsed("24:01") == -1);
    assert(parsed("1441") == -1);
    assert(parsed("12345") == -1);
    assert(parsed("1.5") == -1);
    assert(parsed("1:30:00") == -1);
    assert(parsed("2:001") == -1);
    assert(parsed("2h005") == -1);
    assert(parsed("abc") == -1);
    assert(parsed("90m30") == -1);
    assert(parsed("1m30") == -1);
    assert(parsed("2hm") == -1);
    assert(parsed("90 mins") == -1);
    assert(parsed("1441m") == -1);
    assert(parsed("m") == -1);

    assert(duration::format_hm(90) == "1:30");
    assert(duration::format_hm(45) == "0:45");
    assert(duration::format_hm(1440) == "24:00");
    std::puts("duration parsing assertions passed");
    return 0;
}
