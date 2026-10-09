// pt_block — the raw PlayTimerSettings block (0x44 bytes, core/pure.h) as a
// developer tool: written down once as a reference on the SD card
// (sd:/switch/playguard/logs/play_timer_block.json), then compared with the
// block read later, after one setting was changed in the companion app. The
// values that changed tell which field holds that setting ("alarm only" vs
// "suspend the software"…, or confirm the bedtime fields, core/pure.h).
// Plain C++ (no libnx, no UI) for the host tests (tests/pt_block).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pt_block
{

constexpr size_t WORDS = 34;   // PT_U16_COUNT (core/pure.h)
using Block = std::array<uint16_t, WORDS>;

// 136 upper-case hex digits, four per u16 in order: the backups' "raw_0x44".
std::string to_hex(const Block& b);
// Back from to_hex (either case). False, and *out unchanged, for anything else.
bool from_hex(const std::string& hex, Block* out);

// Where a u16 sits in the layout pure.h documents: "header [0]",
// "[3] (reserved)", or a day's "[+0]", "flag", "minutes", "[+3]" after
// `days[n]` (Sun..Sat, the caller's language).
std::string field_name(size_t index, const std::array<std::string, 7>& days);
const std::array<std::string, 7>& english_days();   // "Sun" … "Sat"

struct Change
{
    size_t   index;
    uint16_t before, after;
};
std::vector<Change> diff(const Block& before, const Block& after);

struct Reference
{
    Block       block{};
    std::string saved_at;   // "2026-10-08 14:03", as the UI wrote it
    std::string firmware;   // "22.1.0"
};

// The report saved with the diagnostics: when the reference was taken, each
// change with its field name, both blocks in hex. English, like the
// diagnostic report.
std::string report(const Reference& ref, const Block& now, const std::string& now_at);

bool save_reference(const Reference& ref, std::string* error = nullptr);
// False when there is none, or when it is damaged (not this format).
bool load_reference(Reference* out);
std::string reference_path();

}   // namespace pt_block
