// config — small persistent settings in sd:/switch/nx_pctl_manager/config.json.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace config
{

struct Config
{
    std::string language = "system";   // "system", "en-US", "fr"
    std::string theme    = "system";   // "system", "light", "dark"
    std::string ntp_server;            // empty: pick from the console region
    std::vector<std::string> custom_servers;
    bool advanced = false;             // show debug-class play-timer actions
    std::string untested_fw_ack;       // firmware for which the "untested" notice was accepted
};

Config& get();
void    load();          // never throws; missing / broken file => defaults
bool    save();          // atomic write; false on failure

}   // namespace config
