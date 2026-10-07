// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/config.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <initializer_list>

#include "util/paths.hpp"

namespace config
{

static Config s_config;

Config& get() { return s_config; }

namespace
{
// One field at a time: a field of the wrong type keeps its default, the
// others are still read (a hand-edited or older file loses one setting, not
// the remembered firmware choice). Each reader returns false when the field is
// there but refused, so related fields can be dropped together.
bool read_string(const nlohmann::json& j, const char* key, std::string& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    if (!it->is_string()) return false;
    out = it->get<std::string>();
    return true;
}

// Exactly a JSON boolean / integer: no 1 for true, no 90.5 for 90.
bool read_bool(const nlohmann::json& j, const char* key, bool& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    if (!it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

bool read_int(const nlohmann::json& j, const char* key, int& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    if (!it->is_number_integer()) return false;
    const long long v = it->get<long long>();
    if (v < -1000000 || v > 1000000) return false;
    out = (int)v;
    return true;
}

void clear_extra(Config& c)
{
    c.extra_weekday = -1;
    c.extra_date.clear();
    c.extra_base = c.extra_value = 0;
}

void one_of(std::string& value, std::initializer_list<const char*> allowed, const char* fallback)
{
    for (const char* a : allowed)
        if (value == a) return;
    value = fallback;
}
}   // namespace

void sanitize(Config& c)
{
    one_of(c.language, { "system", "en-US", "fr" }, "system");
    one_of(c.theme, { "system", "light", "dark" }, "system");
    one_of(c.update_via, { "auto", "sphaira", "appstore", "manual" }, "auto");
    one_of(c.fw_gate_choice, { "", "read_only", "probe", "risk" }, "");
    if (c.fw_gate_choice.empty()) c.fw_gate_fw.clear(), c.fw_gate_app.clear();
    if (c.ntp_server.size() > MAX_HOST) c.ntp_server.clear();

    std::vector<std::string> servers;
    for (const auto& s : c.custom_servers)
        if (!s.empty() && s.size() <= MAX_HOST && servers.size() < MAX_CUSTOM_SERVERS) servers.push_back(s);
    c.custom_servers = servers;

    // A pending extra-time record must describe a real weekday limit, or it
    // could later "put back" a nonsense value (or "no limit").
    const bool weekday_ok = c.extra_weekday >= 0 && c.extra_weekday <= 6;
    const bool values_ok  = c.extra_base >= 0 && c.extra_base <= 1440 && c.extra_value >= 0 &&
                           c.extra_value <= 1440 && c.extra_date.size() == 10;
    if (!weekday_ok || !values_ok) clear_extra(c);
}

void load()
{
    s_config = Config{};
    std::string text;
    if (!paths::read_file(paths::config_file(), text)) return;
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        return;   // not JSON at all: defaults
    }
    if (!j.is_object()) return;

    Config& c = s_config;
    read_string(j, "language", c.language);
    read_string(j, "theme", c.theme);
    read_string(j, "ntp_server", c.ntp_server);
    read_bool(j, "advanced", c.advanced);
    read_bool(j, "auto_relock", c.auto_relock);
    read_bool(j, "dev_mode", c.dev_mode);
    read_string(j, "update_via", c.update_via);
    read_bool(j, "relock_pending", c.relock_pending);

    // The firmware choice and the extra-time record are all-or-nothing.
    const bool gate_ok = read_string(j, "fw_gate_fw", c.fw_gate_fw) &
                         read_string(j, "fw_gate_app", c.fw_gate_app) &
                         read_string(j, "fw_gate_choice", c.fw_gate_choice);
    if (!gate_ok) c.fw_gate_fw.clear(), c.fw_gate_app.clear(), c.fw_gate_choice.clear();
    const bool extra_ok = read_int(j, "extra_weekday", c.extra_weekday) &
                          read_string(j, "extra_date", c.extra_date) &
                          read_int(j, "extra_base", c.extra_base) &
                          read_int(j, "extra_value", c.extra_value);
    if (!extra_ok) clear_extra(c);
    auto servers = j.find("custom_servers");
    if (servers != j.end() && servers->is_array())
        for (const auto& v : *servers)
            if (v.is_string()) c.custom_servers.push_back(v.get<std::string>());
    sanitize(c);
}

bool save()
{
    nlohmann::json j;
    j["schema"]          = SCHEMA;
    j["language"]        = s_config.language;
    j["theme"]           = s_config.theme;
    j["ntp_server"]      = s_config.ntp_server;
    j["custom_servers"]  = s_config.custom_servers;
    j["advanced"]        = s_config.advanced;
    j["auto_relock"]     = s_config.auto_relock;
    j["dev_mode"]        = s_config.dev_mode;
    j["update_via"]      = s_config.update_via;
    j["fw_gate_fw"]      = s_config.fw_gate_fw;
    j["fw_gate_app"]     = s_config.fw_gate_app;
    j["fw_gate_choice"]  = s_config.fw_gate_choice;
    j["extra_weekday"]   = s_config.extra_weekday;
    j["extra_date"]      = s_config.extra_date;
    j["extra_base"]      = s_config.extra_base;
    j["extra_value"]     = s_config.extra_value;
    j["relock_pending"]  = s_config.relock_pending;
    return paths::atomic_write(paths::config_file(),
                               j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n");
}

}   // namespace config
