// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/pt_log.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "util/paths.hpp"

namespace pt_log
{

namespace
{
std::string failed(Result rc)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "!0x%08" PRIX32, (uint32_t)rc);
    return buf;
}

std::string flag(Result rc, bool v) { return R_FAILED(rc) ? failed(rc) : v ? "1" : "0"; }

// Nanoseconds as seconds: "1428", or "1428.5" when not whole (to the ms).
std::string seconds(uint64_t ns)
{
    char buf[32];
    const uint64_t s = ns / 1000000000ULL, ms = (ns % 1000000000ULL) / 1000000ULL;
    if (ns % 1000000000ULL == 0) std::snprintf(buf, sizeof(buf), "%" PRIu64, s);
    else {
        std::snprintf(buf, sizeof(buf), "%" PRIu64 ".%03" PRIu64, s, ms);
        // "1428.500" -> "1428.5"
        size_t n = std::strlen(buf);
        while (buf[n - 1] == '0') buf[--n] = '\0';
    }
    return buf;
}

std::string time_cell(Result rc, uint64_t ns) { return R_FAILED(rc) ? failed(rc) : seconds(ns); }

std::string hex(const void* data, size_t n)
{
    static const char digits[] = "0123456789ABCDEF";
    const auto* b = static_cast<const uint8_t*>(data);
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        out += digits[b[i] >> 4];
        out += digits[b[i] & 15];
    }
    return out;
}

uint64_t le64(const uint8_t* b)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | b[i];
    return v;
}

// The cell, or "" when it is what the line before held.
std::string changed(const std::string& now, std::string* before)
{
    if (now == *before) return "";
    *before = now;
    return now;
}

off_t file_size(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? st.st_size : -1;
}
}   // namespace

std::string path()     { return paths::logs_dir() + "/play_timer_log.csv"; }
std::string old_path() { return paths::logs_dir() + "/play_timer_log.old.csv"; }

std::string header()
{
    return "local_time,posix,unlocked_1006,enabled_1453,restricted_1455,alarm_off_1458,"
           "remaining_s_1454,spent_s_1952,remaining_plus_spent_s,display0_1459,display_remaining_s_1459,"
           "bedtime_1954,extra_s_1960,display_hex_1459,block_hex_145601\n";
}

std::string row(const std::string& local, uint64_t posix, const PtSample& s, Previous* prev)
{
    std::string sum = R_SUCCEEDED(s.remaining_rc) && R_SUCCEEDED(s.spent_rc) ? seconds(s.remaining_ns + s.spent_ns) : "";
    std::string disp0, disp_left, disp_hex;
    if (R_FAILED(s.display_rc)) disp0 = disp_left = failed(s.display_rc);
    else {
        disp0 = std::to_string((unsigned)s.display[0]);
        disp_left = seconds(le64(s.display + 0x10));
        disp_hex = hex(s.display, sizeof(s.display));
    }
    std::string bedtime;
    if (R_FAILED(s.bedtime_rc)) bedtime = failed(s.bedtime_rc);
    else {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%s %02u:%02u", s.bedtime_on ? "on" : "off", (unsigned)s.bedtime_hour,
                      (unsigned)s.bedtime_minute);
        bedtime = buf;
    }
    const std::string block = R_FAILED(s.block_rc) ? failed(s.block_rc) : hex(s.block, sizeof(s.block));

    std::string out = local;
    for (const std::string& cell :
         { std::to_string((unsigned long long)posix), flag(s.unlocked_rc, s.unlocked), flag(s.enabled_rc, s.enabled),
           flag(s.restricted_rc, s.restricted), flag(s.alarm_off_rc, s.alarm_off), time_cell(s.remaining_rc, s.remaining_ns),
           time_cell(s.spent_rc, s.spent_ns), sum, disp0, disp_left, bedtime, time_cell(s.extra_rc, s.extra_ns),
           changed(disp_hex, &prev->display), changed(block, &prev->block) })
        out += "," + cell;
    return out + "\n";
}

std::string tail(const std::string& text, size_t max_bytes)
{
    if (text.size() <= max_bytes) return text;
    const std::string head = header();
    const size_t room = max_bytes > head.size() ? max_bytes - head.size() : 0;
    size_t from = text.size() - room;
    // Start at a whole line: after the first newline at or past `from - 1`.
    const size_t nl = from == 0 ? std::string::npos : text.find('\n', from - 1);
    from = nl == std::string::npos ? text.size() : nl + 1;
    return head + text.substr(from);
}

bool append(const std::string& line, std::string* error)
{
    const std::string p = path();
    if (!paths::ensure_dir(paths::logs_dir())) {
        if (error) *error = std::string("cannot create the logs folder: ") + std::strerror(errno);
        return false;
    }
    if (file_size(p) > (off_t)MAX_BYTES) {
        std::remove(old_path().c_str());
        std::rename(p.c_str(), old_path().c_str());
    }
    const bool fresh = file_size(p) <= 0;
    FILE* f = std::fopen(p.c_str(), "ab");
    if (!f) {
        if (error) *error = std::string("cannot open ") + p + ": " + std::strerror(errno);
        return false;
    }
    const std::string text = fresh ? header() + line : line;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool closed = std::fclose(f) == 0;
    if (!(ok && closed) && error) *error = std::string("cannot write ") + p + ": " + std::strerror(errno);
    return ok && closed;
}

}   // namespace pt_log
