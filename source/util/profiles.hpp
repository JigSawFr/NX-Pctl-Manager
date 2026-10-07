// profiles — named sets of per-day limits saved on the SD card
// (sd:/switch/playguard/profiles/<file>.json), e.g. "School week".
//
// A profile has a display name (any printable text, accents included) and a
// file name derived from it (file_stem: letters, digits, space, '-', '_').
// FAT ignores case, and the stem drops accents, so two names that give the
// same file ("École" and "ecole") are the same profile: find_same_file says
// so before a save replaces anything. Plain C++ (no libnx, no UI) for the
// host tests (tests/profiles).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace profiles
{

constexpr size_t MAX_NAME = 32;   // characters (code points) of a display name

struct Profile
{
    std::string name;               // display name
    std::string file;               // file stem it was read from ("" for a new one)
    std::array<uint16_t, 7> days{}; // Sun..Sat minutes, 0xFFFF == no limit
};

// Every readable profile, by file name. A damaged file, a day out of 0..1440
// or a fraction ("90.5") is skipped; a missing or unusable name becomes the
// file name.
std::vector<Profile> list();

// Writes `p` to the file its name gives. When `p.file` names another file
// (a rename), that one is deleted once the new one is written. Fails with a
// short English reason when the name has nothing usable for a file name.
bool save(const Profile& p, std::string* error = nullptr);

// Deletes the profile stored in `file` (a stem, as in Profile::file).
bool remove(const std::string& file);

// The saved profile, other than the one in `except_file`, that `name` would
// overwrite (same file name, ignoring case). False when there is none.
bool find_same_file(const std::string& name, const std::string& except_file, Profile* out = nullptr);

// Name of the saved profile whose seven days are exactly `days` ("" if none).
// Uses a copy of list() kept until the next save() / remove(), so the state
// header can call it on every refresh without reading the SD card.
std::string match(const uint16_t days[7]);
size_t      count();   // same cache

// A display name: printable characters only (no control characters, no '/'
// or '\\'), spaces trimmed, at most MAX_NAME characters. Invalid UTF-8 is
// dropped. Empty when nothing is left.
std::string sanitize_name(const std::string& name);

// The file name for a display name: accented Latin letters lose their accent
// ("é" -> "e", "œ" -> "oe"), other characters but letters, digits, space,
// '-' and '_' are dropped. Case is kept. Empty when nothing usable is left.
std::string file_stem(const std::string& name);

}   // namespace profiles
