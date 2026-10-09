// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/history.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <climits>

#include "util/backup.hpp"
#include "util/paths.hpp"

namespace history
{

namespace
{
size_t values_for(const std::string& kind)
{
    if (kind == "limits") return 7;
    if (kind == "custom") return 3;
    if (kind == "level" || kind == "org" || kind == "vr" || kind == "alarm") return 1;
    return 0;
}

bool read_values(const nlohmann::json& j, const char* key, std::vector<int>& out)
{
    out.clear();
    auto it = j.find(key);
    if (it == j.end()) return true;   // events have none
    if (!it->is_array()) return false;
    for (const auto& v : *it) {
        // Read as 64-bit: a larger value is refused, never narrowed into a valid one.
        if (!v.is_number_integer()) return false;
        if (v.is_number_unsigned() && v.get<unsigned long long>() > (unsigned long long)INT_MAX) return false;
        const long long n = v.get<long long>();
        if (n < INT_MIN || n > INT_MAX) return false;
        out.push_back((int)n);
    }
    return true;
}

bool in(int v, int lo, int hi) { return v >= lo && v <= hi; }

// Each value is one the console takes for `kind` (the ranges a backup restore
// accepts), so an undo never writes a value from a damaged file.
bool values_ok(const std::string& kind, const std::vector<int>& v)
{
    for (size_t i = 0; i < v.size(); i++) {
        bool ok = false;
        if (kind == "limits") ok = in(v[i], 0, backup::MAX_DAY_MINUTES) || v[i] == backup::DAY_NO_LIMIT;
        else if (kind == "level") ok = in(v[i], 0, backup::MAX_LEVEL);
        else if (kind == "org") ok = in(v[i], 0, backup::MAX_RATING_ORG);
        else if (kind == "custom") ok = i == 0 ? in(v[i], 0, backup::MAX_RATING_AGE) : in(v[i], 0, 1);
        else if (kind == "vr" || kind == "alarm") ok = in(v[i], 0, 1);
        if (!ok) return false;
    }
    return true;
}

nlohmann::json load_array()
{
    std::string text;
    if (!paths::read_file(paths::history_file(), text)) return nlohmann::json::array();
    try {
        nlohmann::json j = nlohmann::json::parse(text);
        if (j.is_object() && j.contains("entries") && j["entries"].is_array()) return j["entries"];
    } catch (...) {
    }
    return nlohmann::json::array();   // damaged: start again
}
}   // namespace

bool append(const Entry& e, std::string* error)
{
    nlohmann::json entries = load_array();
    nlohmann::json j;
    j["when"]   = e.when;
    j["kind"]   = e.kind;
    j["source"] = e.source;
    j["detail"] = e.detail;
    if (!e.before.empty()) j["before"] = e.before;
    if (!e.after.empty()) j["after"] = e.after;
    entries.push_back(j);
    while (entries.size() > MAX_ENTRIES) entries.erase(entries.begin());
    // One change per line: 200 of them stay small and readable.
    std::string text = "{\n \"schema\": 1,\n \"entries\": [";
    for (size_t i = 0; i < entries.size(); i++)
        text += (i ? ",\n  " : "\n  ") + entries[i].dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    text += entries.empty() ? "]\n}\n" : "\n ]\n}\n";
    if (!paths::ensure_dir(paths::data_dir())) {
        if (error) *error = "cannot create the data folder";
        return false;
    }
    return paths::atomic_write(paths::history_file(), text, error);
}

std::vector<Entry> load()
{
    std::vector<Entry> out;
    const nlohmann::json entries = load_array();
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {   // newest first
        const nlohmann::json& j = *it;
        if (!j.is_object()) continue;
        Entry e;
        try {
            if (!j.contains("kind") || !j["kind"].is_string()) continue;
            e.kind   = j["kind"].get<std::string>();
            e.when   = j.value("when", std::string());
            e.source = j.value("source", std::string());
            e.detail = j.value("detail", std::string());
        } catch (...) {
            continue;   // a field of the wrong type
        }
        if (!read_values(j, "before", e.before) || !read_values(j, "after", e.after)) continue;
        out.push_back(e);
    }
    return out;
}

bool undoable(const Entry& e)
{
    const size_t n = values_for(e.kind);
    return n > 0 && e.before.size() == n && e.after.size() == n && values_ok(e.kind, e.before) &&
           values_ok(e.kind, e.after);
}

}   // namespace history
