// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/duration.hpp"

#include <cctype>

namespace duration
{

namespace
{
// Reads 1..4 digits at `i`; false when there are none.
bool read_number(const std::string& s, size_t& i, long* out)
{
    size_t start = i;
    long v = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i]) && i - start < 4) v = v * 10 + (s[i++] - '0');
    if (i == start) return false;
    *out = v;
    return true;
}

void skip_spaces(const std::string& s, size_t& i)
{
    while (i < s.size() && s[i] == ' ') i++;
}

// A minutes unit at `i`: "m", "mn" or "min" (any case). Nothing else is read.
void skip_minute_unit(const std::string& s, size_t& i)
{
    static const char* units[] = { "min", "mn", "m" };
    for (const char* u : units) {
        size_t n = 0;
        while (u[n] && i + n < s.size() && std::tolower((unsigned char)s[i + n]) == u[n]) n++;
        if (!u[n]) {
            i += n;
            return;
        }
    }
}
}   // namespace

bool parse(const std::string& text, uint16_t* minutes)
{
    size_t i = 0;
    skip_spaces(text, i);
    long first = 0;
    if (!read_number(text, i, &first)) return false;
    skip_spaces(text, i);

    long total = first;   // plain minutes: "90", "90m", "90 min"
    const size_t unit_at = i;
    skip_minute_unit(text, i);
    const bool minutes_unit = i != unit_at;
    skip_spaces(text, i);
    if (minutes_unit) {
        if (i != text.size()) return false;   // "90m30"
    } else if (i < text.size()) {
        const char sep = (char)std::tolower((unsigned char)text[i]);
        if (sep != ':' && sep != 'h') return false;
        i++;
        skip_spaces(text, i);
        long mins = 0;
        const size_t digits_at = i;
        bool has_minutes = read_number(text, i, &mins);
        if (sep == ':' && !has_minutes) return false;          // "1:" is a typo
        if (has_minutes && i - digits_at > 2) return false;    // "2:001": two digits at most
        skip_spaces(text, i);
        if (has_minutes) skip_minute_unit(text, i);   // "1h30m", "1h30min"
        skip_spaces(text, i);
        if (i != text.size() || mins > 59) return false;
        total = first * 60 + mins;
    }
    if (total < 0 || total > 1440) return false;
    *minutes = (uint16_t)total;
    return true;
}

std::string format_hm(uint16_t minutes)
{
    std::string mm = std::to_string(minutes % 60);
    if (mm.size() < 2) mm = "0" + mm;
    return std::to_string(minutes / 60) + ":" + mm;
}

}   // namespace duration
