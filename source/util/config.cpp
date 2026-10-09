// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/config.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <initializer_list>
#include <iterator>

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

// "YYYY-MM-DD", digits where they belong.
bool is_date(const std::string& s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : { 0, 1, 2, 3, 5, 6, 8, 9 })
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}
}   // namespace

void sanitize(Config& c)
{
    bool language_ok = false;
    for (const char* l : LANGUAGES) language_ok |= c.language == l;
    if (!language_ok) c.language = "system";
    one_of(c.theme, { "system", "light", "dark" }, "system");
    one_of(c.update_via, { "auto", "sphaira", "appstore", "manual" }, "auto");
    one_of(c.pin_lock, { "off", "changes", "open" }, "off");
    one_of(c.fw_gate_choice, { "", "read_only", "probe", "risk" }, "");
    if (c.fw_gate_choice.empty()) c.fw_gate_fw.clear(), c.fw_gate_app.clear();
    if (c.ntp_server.size() > MAX_HOST) c.ntp_server.clear();
    bool tab_ok = false;
    for (const char* t : START_TABS) tab_ok |= c.start_tab == t;
    if (!tab_ok) c.start_tab = "dashboard";
    bool set_ok = false;
    for (const auto& set : EXTRA_SETS)
        set_ok |= c.extra_amounts == std::vector<int>(std::begin(set), std::end(set));
    if (!set_ok) c.extra_amounts = { 15, 30, 60 };
    if (c.activity_period < 0 || c.activity_period > 2) c.activity_period = 1;
    if (c.export_format < 0 || c.export_format > 3) c.export_format = 0;
    bool keep_ok = false;
    for (int k : BACKUP_KEEP) keep_ok |= c.backup_keep == k;
    if (!keep_ok) c.backup_keep = 0;
    if (!c.update_checked.empty() && c.update_checked.size() != 10) c.update_checked.clear();
    if (!is_date(c.support_reminded)) c.support_reminded.clear();
    if (c.seen_version.size() > 32) c.seen_version.clear();

    std::vector<std::string> servers;
    for (const auto& s : c.custom_servers)
        if (!s.empty() && s.size() <= MAX_HOST && servers.size() < MAX_CUSTOM_SERVERS) servers.push_back(s);
    c.custom_servers = servers;

    // A pending extra-time record must describe a real weekday limit, or it
    // could later "put back" a nonsense value (or "no limit").
    const bool weekday_ok = c.extra_weekday >= 0 && c.extra_weekday <= 6;
    // extra_base may be "no limit" (0xFFFF): No more play today on a day
    // that had no limit puts "no limit" back.
    const bool base_ok    = (c.extra_base >= 0 && c.extra_base <= 1440) || c.extra_base == 0xFFFF;
    const bool values_ok  = base_ok && c.extra_value >= 0 && c.extra_value <= 1440 && c.extra_date.size() == 10;
    if (!weekday_ok || !values_ok) clear_extra(c);

    // The limits console lock saved to put back: exactly seven, each a real
    // limit (0..1440 min) or "no limit" (0xFFFF). Otherwise drop them (turning
    // the lock off then clears the limit rather than restoring nonsense).
    bool prev_ok = c.console_lock_prev.size() == 7;
    for (int v : c.console_lock_prev)
        prev_ok = prev_ok && ((v >= 0 && v <= 1440) || v == 0xFFFF);
    if (!prev_ok) c.console_lock_prev.clear();
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
    read_bool(j, "extra_auto_restore", c.extra_auto_restore);
    read_bool(j, "dev_mode", c.dev_mode);
    read_string(j, "update_via", c.update_via);
    read_bool(j, "update_daily", c.update_daily);
    read_string(j, "update_checked", c.update_checked);
    read_string(j, "start_tab", c.start_tab);
    read_int(j, "activity_period", c.activity_period);
    read_int(j, "export_format", c.export_format);
    read_int(j, "backup_keep", c.backup_keep);
    read_bool(j, "clock_check_at_start", c.clock_check_at_start);
    read_string(j, "pin_lock", c.pin_lock);
    read_bool(j, "onboarding_at_start", c.onboarding_at_start);
    read_bool(j, "support_reminder", c.support_reminder);
    read_string(j, "support_reminded", c.support_reminded);
    read_string(j, "seen_version", c.seen_version);
    read_bool(j, "console_lock", c.console_lock);
    auto prev = j.find("console_lock_prev");
    if (prev != j.end() && prev->is_array()) {
        std::vector<int> v;
        for (const auto& a : *prev)
            if (a.is_number_integer()) v.push_back(a.get<int>());
        c.console_lock_prev = v;   // sanitize() keeps it only when it is seven valid limits
    }
    auto amounts = j.find("extra_amounts");
    if (amounts != j.end() && amounts->is_array()) {
        std::vector<int> v;
        for (const auto& a : *amounts)
            if (a.is_number_integer()) v.push_back(a.get<int>());
        c.extra_amounts = v;   // sanitize() keeps it only when it is one of EXTRA_SETS
    }
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
    j["extra_auto_restore"] = s_config.extra_auto_restore;
    j["dev_mode"]        = s_config.dev_mode;
    j["update_via"]      = s_config.update_via;
    j["update_daily"]    = s_config.update_daily;
    j["update_checked"]  = s_config.update_checked;
    j["start_tab"]       = s_config.start_tab;
    j["extra_amounts"]   = s_config.extra_amounts;
    j["activity_period"] = s_config.activity_period;
    j["export_format"]   = s_config.export_format;
    j["backup_keep"]     = s_config.backup_keep;
    j["clock_check_at_start"] = s_config.clock_check_at_start;
    j["pin_lock"]        = s_config.pin_lock;
    j["onboarding_at_start"] = s_config.onboarding_at_start;
    j["support_reminder"] = s_config.support_reminder;
    j["support_reminded"] = s_config.support_reminded;
    j["seen_version"]    = s_config.seen_version;
    j["console_lock"]    = s_config.console_lock;
    j["console_lock_prev"] = s_config.console_lock_prev;
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
