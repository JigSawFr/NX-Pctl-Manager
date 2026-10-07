// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/backup.hpp"

#include <algorithm>
#include <borealis/extern/nlohmann/json.hpp>
#include <ctime>

#include "util/paths.hpp"

namespace backup
{

namespace
{
constexpr int FORMAT = 1;

// An integer in [lo, hi]; anything else (float, string, out of range) is refused.
bool int_in(const nlohmann::json& j, int lo, int hi, int& out)
{
    if (!j.is_number_integer()) return false;
    const long long v = j.get<long long>();
    if (v < lo || v > hi) return false;
    out = (int)v;
    return true;
}

bool boolean(const nlohmann::json& j, bool& out)
{
    if (!j.is_boolean()) return false;
    out = j.get<bool>();
    return true;
}
}   // namespace

std::string to_json(const Snapshot& s)
{
    nlohmann::json j;
    j["format"]   = FORMAT;
    j["app"]      = "PlayGuard";
    j["created"]  = s.created;
    j["firmware"] = s.firmware;
    nlohmann::json r = nlohmann::json::object();
    if (s.level_ok) r["level"] = s.level;
    if (s.custom_ok)
        r["custom"] = { { "rating_age", s.rating_age },
                        { "sns_post_restricted", s.sns_restricted },
                        { "free_communication_restricted", s.comm_restricted } };
    if (s.vr_ok) r["vr_restricted"] = s.vr_restricted;
    j["restrictions"] = r;
    if (s.days_ok) {
        nlohmann::json days = nlohmann::json::array();
        for (uint16_t d : s.days) {
            if (d == 0xFFFF) days.push_back(nullptr);
            else days.push_back(d);
        }
        j["play_timer"] = { { "days_from_sunday", days } };
    }
    return j.dump(2) + "\n";
}

bool from_json(const std::string& text, Snapshot& out)
{
    Snapshot s;
    try {
        const auto j = nlohmann::json::parse(text);
        int format = 0;
        if (!j.is_object() || !j.contains("format") || !int_in(j.at("format"), FORMAT, FORMAT, format)) return false;
        if (j.contains("created") && j.at("created").is_string()) s.created = j.at("created").get<std::string>();
        if (j.contains("firmware") && j.at("firmware").is_string()) s.firmware = j.at("firmware").get<std::string>();

        if (j.contains("restrictions")) {
            const auto& r = j.at("restrictions");
            if (!r.is_object()) return false;
            if (r.contains("level")) {
                int level = 0;
                if (!int_in(r.at("level"), 0, 4, level)) return false;
                s.level    = (uint32_t)level;
                s.level_ok = true;
            }
            if (r.contains("custom")) {
                const auto& c = r.at("custom");
                int age = 0;
                if (!c.is_object() || !c.contains("rating_age") || !c.contains("sns_post_restricted") ||
                    !c.contains("free_communication_restricted"))
                    return false;
                if (!int_in(c.at("rating_age"), 0, 21, age) ||
                    !boolean(c.at("sns_post_restricted"), s.sns_restricted) ||
                    !boolean(c.at("free_communication_restricted"), s.comm_restricted))
                    return false;
                s.rating_age = (uint8_t)age;
                s.custom_ok  = true;
            }
            if (r.contains("vr_restricted")) {
                if (!boolean(r.at("vr_restricted"), s.vr_restricted)) return false;
                s.vr_ok = true;
            }
        }

        if (j.contains("play_timer")) {
            const auto& pt = j.at("play_timer");
            if (!pt.is_object() || !pt.contains("days_from_sunday")) return false;
            const auto& days = pt.at("days_from_sunday");
            if (!days.is_array() || days.size() != 7) return false;
            for (size_t i = 0; i < 7; i++) {
                int v = 0;
                if (days[i].is_null()) v = 0xFFFF;
                else if (!int_in(days[i], 0, 1440, v)) return false;
                s.days[i] = (uint16_t)v;
            }
            s.days_ok = true;
        }
    } catch (...) {
        return false;
    }
    if (s.empty()) return false;
    out = s;
    return true;
}

std::string save(const Snapshot& s, std::string* error)
{
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char stamp[32] = "unknown";
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tmv);

    const std::string text = to_json(s);
    for (unsigned i = 0; i < 100; ++i) {
        std::string path = paths::backups_dir() + "/" + stamp;
        if (i > 0) path += "_" + std::to_string(i);
        path += ".json";
        std::string existing;
        if (paths::read_file(path, existing)) continue;
        return paths::atomic_write(path, text, error) ? path : "";
    }
    if (error) *error = "No free file name";
    return "";
}

std::vector<std::string> list()
{
    std::vector<std::string> names = paths::list_files(paths::backups_dir(), ".json");
    std::reverse(names.begin(), names.end());   // the stamp sorts oldest first
    return names;
}

bool load(const std::string& name, Snapshot& out)
{
    std::string text;
    return paths::read_file(paths::backups_dir() + "/" + name, text) && from_json(text, out);
}

}   // namespace backup
