// Host tests for source/util/duration.cpp: the forms a daily limit can be typed in.
#include "check.h"
#include <cstdio>

#include "util/duration.hpp"

static int parsed(const char* s)
{
    uint16_t m = 0xFFFF;
    return duration::parse(s, &m) ? m : -1;
}

int main()
{
    CHECK(parsed("90") == 90);
    CHECK(parsed("0") == 0);
    CHECK(parsed("1440") == 1440);
    CHECK(parsed(" 45 ") == 45);
    CHECK(parsed("1:30") == 90);
    CHECK(parsed("0:45") == 45);
    CHECK(parsed("1:05") == 65);
    CHECK(parsed("24:00") == 1440);
    CHECK(parsed("2h") == 120);
    CHECK(parsed("2H30") == 150);
    CHECK(parsed("1 h 30") == 90);
    CHECK(parsed("1h30m") == 90);
    CHECK(parsed("90m") == 90);
    CHECK(parsed("90 min") == 90);
    CHECK(parsed("90 MIN") == 90);
    CHECK(parsed("45mn") == 45);
    CHECK(parsed(" 90 m ") == 90);
    CHECK(parsed("1h30min") == 90);
    CHECK(parsed("1 h 30 min") == 90);
    CHECK(parsed("2 h") == 120);

    CHECK(parsed("") == -1);
    CHECK(parsed(":30") == -1);
    CHECK(parsed("1:") == -1);
    CHECK(parsed("1:60") == -1);
    CHECK(parsed("24:01") == -1);
    CHECK(parsed("1441") == -1);
    CHECK(parsed("12345") == -1);
    CHECK(parsed("1.5") == -1);
    CHECK(parsed("1:30:00") == -1);
    CHECK(parsed("2:001") == -1);
    CHECK(parsed("2h005") == -1);
    CHECK(parsed("abc") == -1);
    CHECK(parsed("90m30") == -1);
    CHECK(parsed("1m30") == -1);
    CHECK(parsed("2hm") == -1);
    CHECK(parsed("90 mins") == -1);
    CHECK(parsed("1441m") == -1);
    CHECK(parsed("m") == -1);

    CHECK(duration::format_hm(90) == "1:30");
    CHECK(duration::format_hm(45) == "0:45");
    CHECK(duration::format_hm(1440) == "24:00");
    return CHECK_DONE("duration parsing assertions passed");
}
