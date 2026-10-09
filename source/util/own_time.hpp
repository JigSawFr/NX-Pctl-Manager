// own_time — when PlayGuard itself was open over a game. Started over a game
// (title override, or a forwarder), PlayGuard runs as that game: the console
// counts the time as play, in its play timer and in the play-event log of the
// account picked at launch. The log cannot be changed, so PlayGuard notes its
// own sessions (user clock, data_dir()/own_time.txt, the last 8 days) and the
// Activity tab leaves them out of today and the last 7 days. Opened from the
// album (an applet), nothing is counted, so nothing is noted.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "util/playlog.h"

namespace own_time
{

// Starts noting this session (when `counted`: running as an application), and
// writes it down every 30 s, so a crash loses at most that much.
void start(bool counted);
// Writes the session's end. At exit.
void stop();

// The noted sessions, this one up to now.
std::vector<PlayLogSpan> spans();

// The file: one "start end" line per session. Reading drops the sessions
// that ended before `keep_after` and the lines that are not one.
inline std::vector<PlayLogSpan> parse(const std::string& text, uint64_t keep_after)
{
    std::vector<PlayLogSpan> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        unsigned long long start = 0, end = 0;
        if (!(fields >> start >> end) || end <= start || end <= keep_after) continue;
        if (end - start > PLAYLOG_MAX_SESSION_S) continue;   // not a session: a broken line
        out.push_back({ start, end });
    }
    return out;
}

inline std::string format(const std::vector<PlayLogSpan>& spans)
{
    std::string out;
    for (const auto& s : spans)
        if (s.end > s.start)
            out += std::to_string((unsigned long long)s.start) + " " + std::to_string((unsigned long long)s.end) + "\n";
    return out;
}

}   // namespace own_time
