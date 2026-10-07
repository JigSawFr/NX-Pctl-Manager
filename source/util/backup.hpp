// backup — snapshots of the parental-control settings saved on the SD card
// (sd:/switch/playguard/backups/<YYYYMMDD_HHMMSS>.json) and restored from the
// Tools tab: restriction level, custom settings, VR mode and the per-day
// play-time limits. The PIN is never saved. Plain C++ (no libnx, no UI) so the
// host tests can run it; reading and writing the console is backup_flow's job.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace backup
{

// Each *_ok flag says the value was read from the console when the backup was
// made: a value that was not is neither saved nor restored.
struct Snapshot
{
    std::string created;    // local time, "2026-10-07 18:30" (shown in the list)
    std::string firmware;   // "23.0.1" (information only)

    bool     level_ok = false;
    uint32_t level    = 0;  // PctlSafetyLevel, 0..4

    bool    custom_ok    = false;   // the settings of the "Custom" level
    uint8_t rating_age   = 0;       // 0 == no age restriction, else up to 21
    bool    sns_restricted  = false;
    bool    comm_restricted = false;

    bool vr_ok         = false;
    bool vr_restricted = false;

    bool days_ok = false;
    std::array<uint16_t, 7> days{};   // Sun..Sat minutes, 0xFFFF == no limit

    // "Time's up" alarm off (1458 / 1953, a debug-class command: restored
    // only with the advanced play-timer actions on). Saved with the limits.
    bool alarm_ok       = false;
    bool alarm_disabled = false;

    // For the record, not restored: the default rating body (1037) and the
    // 0x44 PlayTimerSettings block as read (136 hex digits), to tell later
    // what the fields PlayGuard does not decode held.
    bool        rating_org_ok = false;
    uint32_t    rating_org    = 0;
    std::string raw_block;

    // Nothing a restore would write.
    bool empty() const { return !level_ok && !custom_ok && !vr_ok && !days_ok; }
};

std::string to_json(const Snapshot& s);

// False when the text is not a backup of this format or a value is out of
// range (nothing from a damaged file is ever written to the console).
bool from_json(const std::string& text, Snapshot& out);

// Writes a new file in paths::backups_dir(). Returns its path, or "" and a
// short English reason in *error.
std::string save(const Snapshot& s, std::string* error = nullptr);

// File names (not paths) in paths::backups_dir(), newest first.
std::vector<std::string> list();

bool load(const std::string& name, Snapshot& out);

// Deletes all but the `keep` newest backups (keep 0: deletes nothing).
// Returns how many were deleted.
size_t prune(size_t keep);

}   // namespace backup
