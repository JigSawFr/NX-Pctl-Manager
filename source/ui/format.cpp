// ui — time limits, play time, day names and local dates / times as text.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include <algorithm>
#include <ctime>
#include <fmt/format.h>

using namespace brls::literals;

namespace ui
{

std::string fmt_minutes(uint16_t m)
{
    if (m == PT_DAY_NOLIMIT) return "playguard/common/no_limit"_i18n;
    if (m == 0) return "playguard/common/zero_minutes"_i18n;
    if (m < 60) return brls::getStr("playguard/common/minutes", (int)m);
    if (m % 60 == 0) return brls::getStr("playguard/common/hours", (int)(m / 60));
    return brls::getStr("playguard/common/hours_minutes", (int)(m / 60), fmt::format("{:02d}", (int)(m % 60)));
}

std::string days_summary(const uint16_t days[7])
{
    bool uniform = true, any_nolimit = false;
    int lo = -1, hi = -1;
    for (int d = 0; d < 7; d++) {
        uniform &= days[d] == days[0];
        if (days[d] == PT_DAY_NOLIMIT) {
            any_nolimit = true;
            continue;
        }
        lo = lo < 0 ? days[d] : std::min<int>(lo, days[d]);
        hi = hi < 0 ? days[d] : std::max<int>(hi, days[d]);
    }
    if (uniform)
        return days[0] == PT_DAY_NOLIMIT ? "playguard/common/no_limit"_i18n
                                         : brls::getStr("playguard/play_timer/state/every_day", fmt_minutes(days[0]));
    if (lo < 0) return "playguard/common/no_limit"_i18n;
    // Inside the sentence: "1 h to no limit", not "1 h to No limit".
    const std::string top = any_nolimit ? "playguard/common/no_limit_in_text"_i18n : fmt_minutes((uint16_t)hi);
    return brls::getStr("playguard/play_timer/profile_range", fmt_minutes((uint16_t)lo), top);
}

std::string fmt_played(uint16_t m)
{
    if (m == 0) return brls::getStr("playguard/common/minutes", 0);
    return fmt_minutes(m);
}

std::string fmt_duration_ns(uint64_t ns)
{
    uint64_t minutes = (ns + 30000000000ULL) / 60000000000ULL;
    if (minutes > 1440) minutes = 1440;
    return fmt_played((uint16_t)minutes);   // a time left: "0 min", not "0 min (no play)"
}

std::string fmt_play_time(uint64_t seconds)
{
    if (seconds > 0 && seconds < 60) return "playguard/activity/less_than_minute"_i18n;
    const uint64_t m = (seconds + 30) / 60;
    if (m < 60) return brls::getStr("playguard/common/minutes", (int)m);
    if (m % 60 == 0) return brls::getStr("playguard/common/hours", (unsigned long long)(m / 60));
    return brls::getStr("playguard/common/hours_minutes", (unsigned long long)(m / 60), fmt::format("{:02d}", (int)(m % 60)));
}

std::string day_name(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playguard/days/{}", day));
}

std::string day_name_in_text(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playguard/days_lower/{}", day));
}

std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no)
{
    if (!ok) return "playguard/common/unavailable"_i18n;
    return value ? yes : no;
}

LocalTime local_now()
{
    LocalTime l{};
    u64 posix = 0;
    if (time_local_now(&posix, &l)) return l;
    // No time-zone rule: the C library's idea of local time.
    std::time_t now = (std::time_t)posix;
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    l.year = (uint16_t)(tmv.tm_year + 1900);
    l.month = (uint8_t)(tmv.tm_mon + 1);
    l.day = (uint8_t)tmv.tm_mday;
    l.hour = (uint8_t)tmv.tm_hour;
    l.minute = (uint8_t)tmv.tm_min;
    l.second = (uint8_t)tmv.tm_sec;
    l.wday = (uint8_t)tmv.tm_wday;
    return l;
}

int today_weekday()
{
    return local_now().wday;
}

std::string today_date()
{
    const LocalTime l = local_now();
    return fmt::format("{:04d}-{:02d}-{:02d}", (int)l.year, (int)l.month, (int)l.day);
}

std::string now_hms()
{
    const LocalTime l = local_now();
    return fmt::format("{:02d}:{:02d}:{:02d}", (int)l.hour, (int)l.minute, (int)l.second);
}

std::string now_stamp()
{
    const LocalTime l = local_now();
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}", (int)l.year, (int)l.month, (int)l.day, (int)l.hour, (int)l.minute);
}

std::string level_name(uint32_t level)
{
    if (level > 4) return "?";
    return brls::getStr(fmt::format("playguard/restrictions/levels/{}", level));
}

std::string time_text(uint64_t posix)
{
    if (!posix) return "—";
    char buf[48];
    time_format_local(posix, buf, sizeof(buf));
    return buf;
}

}   // namespace ui
