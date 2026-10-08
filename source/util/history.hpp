// history — what PlayGuard changed on the console, kept on the SD card
// (sd:/switch/playguard/history.json, the newest 200 changes), so a parent
// can see who-did-what from this app and put a value back. Values are stored
// raw (minutes, levels, 0/1) and worded by the UI when shown, in the language
// of the moment. No UI and no console calls: host-tested (tests/history).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace history
{

constexpr size_t MAX_ENTRIES = 200;

struct Entry
{
    std::string when;     // local time, "2026-10-08 18:30"
    // What changed:
    //   "limits"  before/after: 7 values, Sun..Sat minutes (65535 no limit)
    //   "level"   1 value (PctlSafetyLevel)        "org" 1 value (rating body)
    //   "custom"  3 values: age, posting restricted, communication restricted
    //   "vr", "alarm"  1 value, 0/1 (VR restricted / alarm off)
    //   "pin", "unlock", "relock", "unlink", "delete", "clock", "restore": no values
    std::string kind;
    // Where from: "uniform", "day", "per_day", "extra", "stop", "restore_extra",
    // "profile", "remove", "backup", "undo", "first_steps" … ("" when plain).
    std::string source;
    std::string detail;   // a profile's name, the clock's new time …
    std::vector<int> before, after;
};

// Adds `e` (newest) and keeps the newest MAX_ENTRIES. False when the SD card
// refused the write (*error says why).
bool append(const Entry& e, std::string* error = nullptr);

// Every entry, newest first. A missing or damaged file is an empty history;
// a damaged entry is skipped.
std::vector<Entry> load();

// Whether putting `before` back makes sense: a value change of a known kind
// with as many values before as after, as the kind needs.
bool undoable(const Entry& e);

}   // namespace history
