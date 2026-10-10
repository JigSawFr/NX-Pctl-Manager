// config — small persistent settings in sd:/switch/playguard/config.json.
// Every key, its values and default: docs/config.md (keep it in step).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace config
{

struct Config
{
    std::string language = "system";   // "system" or one of LANGUAGES
    std::string theme    = "system";   // "system", "light", "dark"
    std::string ntp_server;            // empty: pick from the console region
    std::vector<std::string> custom_servers;
    bool advanced = false;             // show debug-class play-timer actions
    bool auto_relock = true;           // lock again right after a change that needed an unlock
    bool dev_mode = false;             // developer tools (7 presses on About > Version)
    bool pt_log = false;               // Developer › record the play timer (action/pt_log_flow)
    std::string update_via = "auto";   // "auto", "sphaira", "appstore", "manual"
    bool update_daily = false;         // check for updates at start-up, once a day
    std::string update_checked;        // "YYYY-MM-DD" of the last check, any kind
    std::string start_tab = "dashboard";   // tab shown at start-up (main.xml order: START_TABS)
    std::vector<int> extra_amounts = { 15, 30, 60 };   // "Extra time today" choices, minutes
    int  activity_period = 1;          // Activity's period: 0 today, 1 last 7 days, 2 all time
    int  export_format   = 0;          // last export format (table_export::Format)
    int  backup_keep     = 0;          // backups kept after a new one (0: all of them)
    bool clock_check_at_start = false; // measure the network clock at start-up, say when it is off
    // Security › Ask for the PIN: "off", "changes" (before the first change,
    // then not for 5 min) or "open" (to open PlayGuard). See pin_lock.hpp.
    std::string pin_lock = "off";
    // First steps opens by itself at start-up while no PIN is set, unless
    // its "Show at start-up" switch was turned off.
    bool onboarding_at_start = true;

    // "Support PlayGuard" once a month at start-up (Preferences; off stays
    // off, updates included), and "What's new" once after each update. See
    // util/support.hpp. support_reminded: "YYYY-MM-DD" of the last reminder
    // (or of when the month started); seen_version: the version whose
    // "What's new" was handled.
    bool        support_reminder = true;
    std::string support_reminded;
    std::string seen_version;
    // The agent sysmodule PlayGuard carries (its SHA-256) that the parent
    // answered "Later" to at start-up: not offered again at start (Tools ›
    // Optional modules still offers it).
    std::string agent_update_skipped;

    // Console lock (Security › Console lock): every day's limit set to 0, so a
    // PIN is needed to play. console_lock is whether it is on; console_lock_prev
    // keeps the seven limits it replaced (Sun..Sat minutes, 65535 no limit), to
    // put back when it is turned off. Empty: nothing to put back (the timer was
    // off), so turning it off clears the limit.
    bool             console_lock = false;
    std::vector<int> console_lock_prev;

    // Choice made on the "firmware not supported yet" screen, remembered for
    // one firmware with one app version: "read_only", "probe" or "risk".
    std::string fw_gate_fw;
    std::string fw_gate_app;
    std::string fw_gate_choice;

    // "Extra time today" or "No more play today": the weekday limit changed
    // on extra_date (YYYY-MM-DD) from extra_base to extra_value minutes, for
    // that day only (extra_base may be 0xFFFF, no limit). extra_weekday < 0:
    // none pending.
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
// The languages PlayGuard is translated into, after "system" (the console's
// own): each has resources/i18n/<code>/playguard.json and a name under
// playguard/tools/languages/. tools/check_resources.py checks both; README
// "Translating PlayGuard" lists the steps to add one.
constexpr const char* LANGUAGES[] = { "system", "en-US", "fr", "fr-CA", "de", "es", "es-419", "it", "nl", "pt", "pt-BR", "ru", "ja", "ko", "zh-Hans", "zh-Hant" };
// What start_tab may be, in the sidebar's order.
constexpr const char* START_TABS[] = { "dashboard", "play_timer", "activity", "restrictions",
                                       "clock", "security", "preferences", "tools", "about" };
// What extra_amounts may be (the Tools picker offers these sets).
constexpr int EXTRA_SETS[][3] = { { 15, 30, 60 }, { 10, 20, 30 }, { 30, 60, 90 }, { 5, 10, 15 } };
constexpr int BACKUP_KEEP[]   = { 0, 5, 10, 20 };
constexpr size_t MAX_HOST           = 253;  // longest DNS name
constexpr size_t MAX_CUSTOM_SERVERS = 10;

Config& get();
void    load();          // never throws; missing / broken file => defaults, per field
bool    save();          // atomic write; false on failure
// Runs after every successful save() (the remote link mirrors what the agent
// must know into sync/nro_state.txt). nullptr: nothing.
void    set_saved_hook(void (*hook)());
// Puts every out-of-range value back to its default (load() calls it).
void    sanitize(Config& c);

}   // namespace config
