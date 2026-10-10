// outside_change_logic — changes made outside PlayGuard, as far as PlayGuard
// can see them from one reading to the next: today's play time starting over
// (a clock change resets it, docs/parental-controls.md), the console clock
// moved against the steady clock, the limits differing from the ones
// PlayGuard last knew. Nothing runs in the background: only what changed
// while PlayGuard was open, or since it last was, is seen. The record is
// kept in watch.json next to config.json (docs/config.md). Plain C++ so the
// host tests can run it (tests/outside_change_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace outside_change_logic
{

// Time spent going down by less is not a reset (rounding of the minutes,
// the seconds between the two reads it comes from).
constexpr int64_t SPENT_SLACK_S = 60;
// The user clock moving against the steady clock by more is a clock change
// (the automatic correction only moves it by seconds).
constexpr int64_t CLOCK_SLACK_S = 300;
// The time spent is written back at most this often (it moves every second).
constexpr int64_t SPENT_SAVE_S = 300;
// The clock offset is written back when it drifted at least this much.
constexpr int64_t OFFSET_SAVE_S = 60;

// What the Overview says, as bits.
constexpr unsigned NOTICE_RESET  = 1;   // today's play time started over
constexpr unsigned NOTICE_CLOCK  = 2;   // the console clock was changed
constexpr unsigned NOTICE_LIMITS = 4;   // the limits were changed outside PlayGuard

struct Record
{
    std::string date;               // the day spent_s belongs to, "YYYY-MM-DD"
    int64_t     spent_s = -1;       // the time spent last seen that day; -1 unknown
    int64_t     saved_spent_s = -1; // what the file holds (not written itself)
    bool        offset_known = false;
    int64_t     offset_s = 0;       // user clock minus steady clock, seconds
    std::string steady_id;          // the steady clock's source: another one is not compared
    bool        limits_known = false;
    uint16_t    limits[7] = {};     // Sun..Sat minutes, 65535 no limit
    unsigned    notice = 0;         // NOTICE_* bits shown on the Overview
    std::string notice_date;        // the day they were seen: cleared the next day
    std::string reset_at;           // "HH:MM" the play time started over
};

struct Observation
{
    std::string date;               // today, "YYYY-MM-DD"
    std::string hm;                 // now, "HH:MM"
    bool        spent_known = false;
    int64_t     spent_s = 0;
    bool        clock_known = false;
    int64_t     offset_s = 0;
    std::string steady_id;
    bool        limits_known = false;
    uint16_t    limits[7] = {};
};

struct Findings
{
    bool     reset = false, clock = false, limits = false;
    int64_t  spent_before_s = 0;    // reset: what was counted before
    int64_t  clock_moved_s = 0;     // clock: by how much (+ forward)
    uint16_t limits_before[7] = {}, limits_after[7] = {};
    bool     save = false;          // the record changed: write it
};

// Compares a reading with the record, and updates the record. A first
// reading of a value (or after own_change) only records it.
Findings check(Record& rec, const Observation& ob);

// PlayGuard itself just changed the limits, the clock or everything: what
// it reads next is taken as is, not as a change made outside.
void own_change(Record& rec);

// The Overview's notice was dismissed.
void dismiss(Record& rec);

// watch.json. parse() never fails: what is missing or out of range stays
// unknown (a first reading), and a damaged file is a fresh record.
Record      parse(const std::string& text);
std::string serialize(const Record& rec);

}   // namespace outside_change_logic
