// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/play_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace play_cache
{

namespace
{
constexpr char MAGIC[4] = { 'P', 'G', 'P', 'C' };
constexpr uint32_t VERSION = 1;

struct Header
{
    char magic[4];
    uint32_t version;
    uint32_t stats_size;   // sizeof(PlayStats), sizeof(GameStat): another
    uint32_t game_size;    // layout (another build) is not read
};

// The stats without their games array, then `count` games.
constexpr size_t HEAD_SIZE = offsetof(PlayStats, games);
}   // namespace

std::string encode(const PlayStats& stats)
{
    const uint32_t count = stats.count > PLAYSTATS_MAX ? PLAYSTATS_MAX : stats.count;
    Header h;
    std::memcpy(h.magic, MAGIC, sizeof MAGIC);
    h.version    = VERSION;
    h.stats_size = sizeof(PlayStats);
    h.game_size  = sizeof(GameStat);
    std::string out;
    out.append(reinterpret_cast<const char*>(&h), sizeof h);
    out.append(reinterpret_cast<const char*>(&stats), HEAD_SIZE);
    out.append(reinterpret_cast<const char*>(stats.games), count * sizeof(GameStat));
    return out;
}

bool decode(const std::string& bytes, PlayStats& out)
{
    Header h;
    if (bytes.size() < sizeof h + HEAD_SIZE) return false;
    std::memcpy(&h, bytes.data(), sizeof h);
    if (std::memcmp(h.magic, MAGIC, sizeof MAGIC) != 0 || h.version != VERSION ||
        h.stats_size != sizeof(PlayStats) || h.game_size != sizeof(GameStat))
        return false;
    std::memset(&out, 0, sizeof out);
    std::memcpy(&out, bytes.data() + sizeof h, HEAD_SIZE);
    if (out.count > PLAYSTATS_MAX || bytes.size() != sizeof h + HEAD_SIZE + out.count * sizeof(GameStat)) {
        std::memset(&out, 0, sizeof out);
        return false;
    }
    std::memcpy(out.games, bytes.data() + sizeof h + HEAD_SIZE, out.count * sizeof(GameStat));
    for (uint32_t i = 0; i < out.count; i++) out.games[i].name[PLAYSTATS_NAME_LEN - 1] = '\0';
    return true;
}

void shift_days(PlayStats& stats, int days)
{
    if (days == 0 || !stats.windows_ok) return;
    if (days < 0) {
        stats.windows_ok = false;
        return;
    }
    const int wday0 = stats.day_wday[0];
    for (int k = 0; k < 7; k++) stats.day_wday[k] = (uint8_t)(((wday0 + days - k) % 7 + 7) % 7);
    for (uint32_t i = 0; i < stats.count; i++) {
        GameStat& g = stats.games[i];
        uint32_t week = 0;
        for (int k = 6; k >= 0; k--) {
            g.day_s[k] = k >= days ? g.day_s[k - days] : 0;
            week += g.day_s[k];
        }
        g.today_s = g.day_s[0];
        g.week_s  = week;
    }
}

}   // namespace play_cache
