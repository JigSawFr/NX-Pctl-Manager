// PlayTimerPerDayActivity — per-day limits editor. Each day cell stages an edit
// into `pending[]` (a quick value, "Enter minutes…" or "No limit"); presets
// fill several days at once; "Save" writes the seven values through the
// play-timer gate. X re-reads the state, B leaves (asking first when there are
// unsaved edits). The same screen edits a saved profile: it then starts from
// the profile's days and "Save" hands them back instead of writing anything.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <borealis.hpp>
#include <functional>
#include <string>

#include "util/pctl_ops_c.hpp"
#include "view/pt_state_header.hpp"

class PlayTimerPerDayActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/play_timer_perday.xml");

    using Days = std::array<u16, 7>;

    PlayTimerPerDayActivity() = default;
    // Profile mode: `title` names the screen, `days` are the starting point,
    // `on_save` stores the result and returns true when it did (the screen
    // then closes). Nothing is written to the console.
    PlayTimerPerDayActivity(std::string title, const Days& days, std::function<bool(const Days&)> on_save);

    void onContentAvailable() override;

  private:
    bool profile_mode = false;
    std::string profile_title;
    std::function<bool(const Days&)> on_profile_save;

    PtState live = {};
    u16     pending[7] = { PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT,
                           PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT };

    void reload_from_service();
    void rerender();
    bool has_changes() const;
    void edit_day(int d);
    void fill_days(std::initializer_list<int> days, const std::string& title);
    void pick_limit(const std::string& title, u16 current, std::function<void(u16)> on_value);
    void save();

    brls::DetailCell* day_cell(int d);

    BRLS_BIND(PtStateHeader,    state_header, "pt_state_header");
    BRLS_BIND(brls::DetailCell, pt_d0,        "pt_d0");
    BRLS_BIND(brls::DetailCell, pt_d1,        "pt_d1");
    BRLS_BIND(brls::DetailCell, pt_d2,        "pt_d2");
    BRLS_BIND(brls::DetailCell, pt_d3,        "pt_d3");
    BRLS_BIND(brls::DetailCell, pt_d4,        "pt_d4");
    BRLS_BIND(brls::DetailCell, pt_d5,        "pt_d5");
    BRLS_BIND(brls::DetailCell, pt_d6,        "pt_d6");
    BRLS_BIND(brls::DetailCell, pt_weekdays,  "pt_weekdays");
    BRLS_BIND(brls::DetailCell, pt_weekend,   "pt_weekend");
    BRLS_BIND(brls::DetailCell, pt_copy_today,"pt_copy_today");
    BRLS_BIND(brls::DetailCell, pt_revert,    "pt_revert");
    BRLS_BIND(brls::DetailCell, pt_save,      "pt_save");
};
