// pt_log — the play-timer recorder of the developer tools: every 30 s while
// PlayGuard is open, one CSV line of what the play timer reports (pctl
// PtSample: 1006, 1453, 1455, 1458, 1454, 1952, 1459, 1954/1956/1957, 1960 and
// the 145601 block), in logs/play_timer_log.csv. Left running over midnight,
// or until the time is up, it answers what one report cannot: when the time
// spent resets, what 1459 says near the end (docs/parental-controls.md).
// This part is plain C++ (no libnx, no UI) for the host tests
// (tests/pt_log); action/pt_log_flow runs it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
#include "core/pctl_ops.h"
}

namespace pt_log
{

// Past this size the file becomes play_timer_log.old.csv (replacing the
// previous one) and a new one starts: two days at one line every 30 s.
constexpr size_t MAX_BYTES = 2 * 1024 * 1024;

std::string path();       // logs_dir()/play_timer_log.csv
std::string old_path();   // logs_dir()/play_timer_log.old.csv

// The first line of the file.
std::string header();

// What the line before held, so the two long hex columns (1459 and the
// block) are written only when they change: empty means "as above".
struct Previous
{
    std::string display, block;
};

// One line (ending in "\n") for the reading `s` taken at `posix` (user clock,
// UTC seconds), `local` being the same time as the console shows it
// ("2026-10-09 20:10:52"). A failed read is written "!" and its result code
// ("!0x0001188E"); times are in seconds, decimals only when not whole.
// `event`, the last column, says why this line was written when it was not
// the 30 s tick (an action of PlayGuard, the clock moving…), quoted as CSV
// needs.
std::string row(const std::string& local, uint64_t posix, const PtSample& s, Previous* prev,
                const std::string& event = "");

// The user clock against the time that really went by (a steady clock, in
// seconds) since the last reading: "clock +1411 s vs elapsed" when they
// differ by CLOCK_SLACK_S or more (the clock was changed, or the console
// slept), "" otherwise and on the first reading. Updates *last.
constexpr int64_t CLOCK_SLACK_S = 5;
struct Clock
{
    bool     known = false;
    uint64_t posix = 0, steady_s = 0;
};
std::string clock_moved(Clock* last, uint64_t posix, uint64_t steady_s);

// The end of `text` (a CSV of this format) in at most `max_bytes`, cut at a
// line start, with the header line put back in front when it was cut off.
std::string tail(const std::string& text, size_t max_bytes);

// Appends `line` to path(), writing the header first in a new file, and moves
// the file to old_path() once it is over MAX_BYTES, or when it starts with
// another header (columns of an earlier version). False, and a short
// English reason in *error, when the SD card refused.
bool append(const std::string& line, std::string* error = nullptr);

}   // namespace pt_log
