// profiles — named sets of per-day limits saved on the SD card
// (sd:/switch/playguard/profiles/<name>.json), e.g. "School week".
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace profiles
{

struct Profile
{
    std::string name;
    std::array<uint16_t, 7> days;   // Sun..Sat minutes, 0xFFFF == no limit
};

std::vector<Profile> list();
bool save(const Profile& p, std::string* error = nullptr);
bool remove(const std::string& name);

// Name of the saved profile whose seven days are exactly `days` ("" if none).
// Uses a copy of list() kept until the next save() / remove(), so the state
// header can call it on every refresh without reading the SD card.
std::string match(const uint16_t days[7]);
size_t      count();   // same cache

// Keeps letters, digits, space, '-' and '_' (max 32 chars) so the name is a
// valid FAT file name. Returns empty if nothing usable is left.
std::string sanitize_name(const std::string& name);

}   // namespace profiles
