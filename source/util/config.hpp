// config — small persistent settings in sd:/switch/playguard/config.json.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
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
    bool auto_relock = true;           // lock again right after a change that needed an unlock
    bool dev_mode = false;             // developer tools (7 presses on Tools > Version)
    std::string update_via = "auto";   // "auto", "sphaira", "appstore", "manual"

    // Choice made on the "firmware not supported yet" screen, remembered for
    // one firmware with one app version: "read_only", "probe" or "risk".
    std::string fw_gate_fw;
    std::string fw_gate_app;
    std::string fw_gate_choice;

    // "Extra time today": the weekday limit raised on extra_date (YYYY-MM-DD)
    // from extra_base to extra_value minutes. extra_weekday < 0: none pending.
    int         extra_weekday = -1;
    std::string extra_date;
    int         extra_base  = 0;
    int         extra_value = 0;

    // Set right before PlayGuard unlocks parental controls for a change it
    // locks again afterwards, cleared once it has. Still set at the next start:
    // the app stopped in between, so it locks again then.
    bool relock_pending = false;
};

constexpr int    SCHEMA             = 1;    // "schema" in config.json
constexpr size_t MAX_HOST           = 253;  // longest DNS name
constexpr size_t MAX_CUSTOM_SERVERS = 10;

Config& get();
void    load();          // never throws; missing / broken file => defaults, per field
bool    save();          // atomic write; false on failure
// Puts every out-of-range value back to its default (load() calls it).
void    sanitize(Config& c);

}   // namespace config
