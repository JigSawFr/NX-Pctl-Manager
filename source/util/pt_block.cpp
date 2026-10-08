// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/pt_block.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <cstdio>

#include "util/paths.hpp"

namespace pt_block
{

namespace
{
std::string word_hex(uint16_t w)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04X", (unsigned)w);
    return buf;
}

int digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}   // namespace

std::string to_hex(const Block& b)
{
    std::string out;
    out.reserve(WORDS * 4);
    for (uint16_t w : b) out += word_hex(w);
    return out;
}

bool from_hex(const std::string& hex, Block* out)
{
    if (hex.size() != WORDS * 4) return false;
    Block b{};
    for (size_t i = 0; i < WORDS; i++) {
        unsigned w = 0;
        for (size_t k = 0; k < 4; k++) {
            const int d = digit(hex[i * 4 + k]);
            if (d < 0) return false;
            w = (w << 4) | (unsigned)d;
        }
        b[i] = (uint16_t)w;
    }
    if (out) *out = b;
    return true;
}

const std::array<std::string, 7>& english_days()
{
    static const std::array<std::string, 7> days = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return days;
}

std::string field_name(size_t index, const std::array<std::string, 7>& days)
{
    if (index < 2) return "header [" + std::to_string(index) + "]";
    if (index < 7) return "[" + std::to_string(index) + "] (reserved)";
    if (index >= WORDS) return "[" + std::to_string(index) + "]";
    static const char* const PARTS[4] = { "[+0]", "flag", "minutes", "[+3]" };
    const size_t day = (index - 7) / 4;
    return days[day] + " " + PARTS[(index - 7) % 4];
}

std::vector<Change> diff(const Block& before, const Block& after)
{
    std::vector<Change> out;
    for (size_t i = 0; i < WORDS; i++)
        if (before[i] != after[i]) out.push_back({ i, before[i], after[i] });
    return out;
}

std::string report(const Reference& ref, const Block& now, const std::string& now_at)
{
    std::string text = "PlayGuard - play-timer block (145601 GetPlayTimerSettings, 34 x u16)\n";
    text += "Reference: " + ref.saved_at + (ref.firmware.empty() ? "" : " (firmware " + ref.firmware + ")") + "\n";
    text += "Now:       " + now_at + "\n\n";
    const auto changes = diff(ref.block, now);
    if (changes.empty()) text += "No value changed.\n";
    for (const auto& c : changes) {
        char line[128];
        std::snprintf(line, sizeof(line), "  u16[%2u] %-16s %s -> %s  (%u -> %u)\n", (unsigned)c.index,
                      field_name(c.index, english_days()).c_str(), word_hex(c.before).c_str(), word_hex(c.after).c_str(),
                      (unsigned)c.before, (unsigned)c.after);
        text += line;
    }
    text += "\nreference " + to_hex(ref.block) + "\nnow       " + to_hex(now) + "\n";
    return text;
}

std::string reference_path()
{
    return paths::logs_dir() + "/play_timer_block.json";
}

bool save_reference(const Reference& ref, std::string* error)
{
    nlohmann::json j;
    j["format"]   = "playguard-play-timer-block";
    j["saved_at"] = ref.saved_at;
    j["firmware"] = ref.firmware;
    j["block"]    = to_hex(ref.block);
    return paths::atomic_write(reference_path(), j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n",
                               error);
}

bool load_reference(Reference* out)
{
    std::string text;
    if (!paths::read_file(reference_path(), text)) return false;
    try {
        const auto j = nlohmann::json::parse(text);
        if (!j.is_object() || j.value("format", "") != "playguard-play-timer-block") return false;
        Reference r;
        const auto block = j.find("block");
        if (block == j.end() || !block->is_string() || !from_hex(block->get<std::string>(), &r.block)) return false;
        const auto saved = j.find("saved_at");
        if (saved != j.end() && saved->is_string()) r.saved_at = saved->get<std::string>();
        const auto fw = j.find("firmware");
        if (fw != j.end() && fw->is_string()) r.firmware = fw->get<std::string>();
        if (out) *out = r;
        return true;
    } catch (...) {
        return false;
    }
}

}   // namespace pt_block
