// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/profiles.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <cstdio>

#include "util/paths.hpp"

namespace profiles
{

namespace
{
std::vector<Profile> g_cache;
bool g_cache_valid = false;
}   // namespace

std::string sanitize_name(const std::string& name)
{
    std::string out;
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == ' ' || c == '-' || c == '_')
            out += (char)c;
        if (out.size() >= 32) break;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out;
}

std::vector<Profile> list()
{
    std::vector<Profile> out;
    for (const auto& file : paths::list_files(paths::profiles_dir(), ".json")) {
        std::string text;
        if (!paths::read_file(paths::profiles_dir() + "/" + file, text)) continue;
        try {
            auto j = nlohmann::json::parse(text);
            Profile p;
            p.name = j.value("name", file.substr(0, file.size() - 5));
            auto days = j.at("days");
            if (!days.is_array() || days.size() != 7) continue;
            bool ok = true;
            for (int i = 0; i < 7; i++) {
                if (days[i].is_null()) { p.days[i] = 0xFFFF; continue; }
                int v = days[i].get<int>();
                if (v < 0 || v > 1440) { ok = false; break; }
                p.days[i] = (uint16_t)v;
            }
            if (ok) out.push_back(p);
        } catch (...) {
            // ignore unreadable profile files
        }
    }
    return out;
}

bool save(const Profile& p, std::string* error)
{
    std::string name = sanitize_name(p.name);
    if (name.empty()) {
        if (error) *error = "Invalid name";
        return false;
    }
    nlohmann::json j;
    j["name"] = name;
    j["days"] = nlohmann::json::array();
    for (auto d : p.days) {
        if (d == 0xFFFF) j["days"].push_back(nullptr);
        else j["days"].push_back(d);
    }
    g_cache_valid = false;
    return paths::atomic_write(paths::profiles_dir() + "/" + name + ".json", j.dump(2) + "\n", error);
}

static const std::vector<Profile>& cached()
{
    if (!g_cache_valid) {
        g_cache = list();
        g_cache_valid = true;
    }
    return g_cache;
}

size_t count()
{
    return cached().size();
}

std::string match(const uint16_t days[7])
{
    for (const auto& p : cached()) {
        bool same = true;
        for (int i = 0; i < 7 && same; i++) same = p.days[i] == days[i];
        if (same) return p.name;
    }
    return "";
}

bool remove(const std::string& name)
{
    std::string n = sanitize_name(name);
    g_cache_valid = false;
    return !n.empty() && std::remove((paths::profiles_dir() + "/" + n + ".json").c_str()) == 0;
}

}   // namespace profiles
