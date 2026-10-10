// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/pt_log.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

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

// Opened for appending (and reading), created 0644 (as util/http.cpp), not
// fopen's 0666.
FILE* open_append(const std::string& p)
{
    const int fd = ::open(p.c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return nullptr;
    FILE* f = ::fdopen(fd, "a+b");
    if (!f) ::close(fd);
    return f;
}

// Whether the open file must be put aside: over MAX_BYTES, or begun with
// another header (an earlier version's columns). *size: its size.
bool must_roll(FILE* f, long* size)
{
    *size = std::fseek(f, 0, SEEK_END) == 0 ? std::ftell(f) : -1;
    if (*size > (long)MAX_BYTES) return true;
    if (*size <= 0) return false;
    const std::string head = header();
    std::string first(head.size(), '\0');
    std::rewind(f);
    const size_t n = std::fread(&first[0], 1, first.size(), f);
    std::fseek(f, 0, SEEK_END);
    return n != head.size() || first != head;
}

// A CSV cell: quoted (quotes doubled) when it holds a comma, a quote or a
// line break.
std::string csv(const std::string& text)
{
    if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
    std::string out = "\"";
    for (char c : text) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

// The cell, or "" when it is what the line before held.
std::string changed(const std::string& now, std::string* before)
{
    if (now == *before) return "";
    *before = now;
    return now;
}

}   // namespace

std::string path()     { return paths::logs_dir() + "/play_timer_log.csv"; }
std::string old_path() { return paths::logs_dir() + "/play_timer_log.old.csv"; }

std::string header()
{
    return "local_time,posix,unlocked_1006,enabled_1453,restricted_1455,alarm_off_1458,"
           "remaining_s_1454,spent_s_1952,remaining_plus_spent_s,display0_1459,display_remaining_s_1459,"
           "bedtime_1954,extra_s_1960,display_hex_1459,block_hex_145601,event\n";
}

std::string row(const std::string& local, uint64_t posix, const PtSample& s, Previous* prev,
                const std::string& event)
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
           changed(disp_hex, &prev->display), changed(block, &prev->block), csv(event) })
        out += "," + cell;
    return out + "\n";
}

std::string clock_moved(Clock* last, uint64_t posix, uint64_t steady_s)
{
    std::string out;
    if (last->known) {
        const int64_t gap = ((int64_t)posix - (int64_t)last->posix) - ((int64_t)steady_s - (int64_t)last->steady_s);
        if (gap >= CLOCK_SLACK_S || gap <= -CLOCK_SLACK_S) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "clock %+" PRId64 " s vs elapsed", gap);
            out = buf;
        }
    }
    *last = { true, posix, steady_s };
    return out;
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
    // The size is read from the open file, not from the path beforehand.
    FILE* f = open_append(p);
    long size = -1;
    if (f && must_roll(f, &size)) {
        std::fclose(f);
        std::remove(old_path().c_str());
        std::rename(p.c_str(), old_path().c_str());
        f = open_append(p);
        if (f) must_roll(f, &size);
    }
    if (!f) {
        if (error) *error = std::string("cannot open ") + p + ": " + std::strerror(errno);
        return false;
    }
    const std::string text = size <= 0 ? header() + line : line;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool closed = std::fclose(f) == 0;
    if (!(ok && closed) && error) *error = std::string("cannot write ") + p + ": " + std::strerror(errno);
    return ok && closed;
}

}   // namespace pt_log
