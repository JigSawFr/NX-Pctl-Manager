// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/config.hpp"

#include <borealis/extern/nlohmann/json.hpp>

#include "util/paths.hpp"

namespace config
{

static Config s_config;

Config& get() { return s_config; }

void load()
{
    s_config = Config{};
    std::string text;
    if (!paths::read_file(paths::config_file(), text)) return;
    try {
        auto j = nlohmann::json::parse(text);
        s_config.language        = j.value("language", s_config.language);
        s_config.theme           = j.value("theme", s_config.theme);
        s_config.ntp_server      = j.value("ntp_server", s_config.ntp_server);
        s_config.advanced        = j.value("advanced", s_config.advanced);
        s_config.auto_relock     = j.value("auto_relock", s_config.auto_relock);
        s_config.dev_mode        = j.value("dev_mode", s_config.dev_mode);
        s_config.update_via      = j.value("update_via", s_config.update_via);
        s_config.fw_gate_fw      = j.value("fw_gate_fw", s_config.fw_gate_fw);
        s_config.fw_gate_app     = j.value("fw_gate_app", s_config.fw_gate_app);
        s_config.fw_gate_choice  = j.value("fw_gate_choice", s_config.fw_gate_choice);
        s_config.extra_weekday   = j.value("extra_weekday", s_config.extra_weekday);
        s_config.extra_date      = j.value("extra_date", s_config.extra_date);
        s_config.extra_base      = j.value("extra_base", s_config.extra_base);
        s_config.extra_value     = j.value("extra_value", s_config.extra_value);
        if (j.contains("custom_servers") && j["custom_servers"].is_array())
            for (auto& v : j["custom_servers"])
                if (v.is_string()) s_config.custom_servers.push_back(v.get<std::string>());
    } catch (...) {
        s_config = Config{};   // corrupt file: start over with defaults
    }
}

bool save()
{
    nlohmann::json j;
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
    return paths::atomic_write(paths::config_file(), j.dump(2) + "\n");
}

}   // namespace config
