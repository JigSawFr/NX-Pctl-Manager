// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/support.hpp"

#include <cctype>

namespace support
{
namespace
{
bool parse_date(const std::string& s, int* y, int* m, int* d)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : { 0, 1, 2, 3, 5, 6, 8, 9 })
        if (!std::isdigit((unsigned char)s[i])) return false;
    *y = std::stoi(s.substr(0, 4));
    *m = std::stoi(s.substr(5, 2));
    *d = std::stoi(s.substr(8, 2));
    return *m >= 1 && *m <= 12 && *d >= 1 && *d <= 31;
}

// Days since 1970-01-01 (Howard Hinnant's days_from_civil).
long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const long     era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}
}   // namespace

bool days_between(const std::string& a, const std::string& b, int* days)
{
    int ya, ma, da, yb, mb, db;
    if (!parse_date(a, &ya, &ma, &da) || !parse_date(b, &yb, &mb, &db)) return false;
    *days = (int)(days_from_civil(yb, mb, db) - days_from_civil(ya, ma, da));
    return true;
}

Decision decide(const State& state, const std::string& version, const std::string& today, bool has_notes)
{
    Decision d;
    d.next = state;
    if (state.seen_version != version) {
        if (!state.seen_version.empty() && has_notes) d.show = Show::WhatsNew;
        d.next.seen_version = version;
        d.next.reminded     = today;
        return d;
    }
    int days = 0;
    if (!days_between(state.reminded, today, &days) || days < 0) {
        d.next.reminded = today;   // not counting yet, or the clock went back
        return d;
    }
    if (state.reminder_on && days >= REMINDER_DAYS) {
        d.show          = Show::Reminder;
        d.next.reminded = today;
    }
    return d;
}

}   // namespace support
