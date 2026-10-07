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
    bool update_daily = false;         // check for updates at start-up, once a day
    std::string update_checked;        // "YYYY-MM-DD" of the last check, any kind
    std::string start_tab = "dashboard";   // tab shown at start-up (main.xml order: START_TABS)
    std::vector<int> extra_amounts = { 15, 30, 60 };   // "Extra time today" choices, minutes
    int  activity_period = 1;          // Activity's period: 0 today, 1 last 7 days, 2 all time
    int  export_format   = 0;          // last export format (table_export::Format)
    int  backup_keep     = 0;          // backups kept after a new one (0: all of them)
    bool clock_check_at_start = false; // measure the network clock at start-up, say when it is off

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
    // Put that limit back by itself the next day (when the timer needs the
    // temporary unlock, that is still asked) instead of asking keep / put back.
    bool        extra_auto_restore = false;

    // Set right before PlayGuard unlocks parental controls for a change it
    // locks again afterwards, cleared once it has. Still set at the next start:
    // the app stopped in between, so it locks again then.
    bool relock_pending = false;
};

constexpr int    SCHEMA             = 1;    // "schema" in config.json
// What start_tab may be, in the sidebar's order.
constexpr const char* START_TABS[] = { "dashboard", "play_timer", "activity", "restrictions",
                                       "clock", "security", "tools" };
// What extra_amounts may be (the Tools picker offers these sets).
constexpr int EXTRA_SETS[][3] = { { 15, 30, 60 }, { 10, 20, 30 }, { 30, 60, 90 }, { 5, 10, 15 } };
constexpr int BACKUP_KEEP[]   = { 0, 5, 10, 20 };
constexpr size_t MAX_HOST           = 253;  // longest DNS name
constexpr size_t MAX_CUSTOM_SERVERS = 10;

Config& get();
void    load();          // never throws; missing / broken file => defaults, per field
bool    save();          // atomic write; false on failure
// Puts every out-of-range value back to its default (load() calls it).
void    sanitize(Config& c);

}   // namespace config
