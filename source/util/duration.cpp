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
}   // namespace

bool parse(const std::string& text, uint16_t* minutes)
{
    size_t i = 0;
    skip_spaces(text, i);
    long first = 0;
    if (!read_number(text, i, &first)) return false;
    skip_spaces(text, i);

    long total = first;   // plain minutes
    if (i < text.size()) {
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
        if (i < text.size() && std::tolower((unsigned char)text[i]) == 'm') i++;   // "1h30m"
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
