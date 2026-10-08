// GameActivity — one game of the Activity tab on its own screen: icon and
// name, the last seven days as bars, today / 7 days / all time, launches,
// first and last play, and the all-time play time of each user account (read
// when the screen opens: a few requests for this game only).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <memory>
#include <vector>

#include "util/pctl_ops_c.hpp"
#include "view/play_days.hpp"

class GameActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/game.xml");

    // `stats` is the Activity tab's read (for the weekday names and whether
    // the 7-day figures are meaningful); `icon` may be empty.
    GameActivity(GameStat game, std::shared_ptr<const PlayStats> stats, std::vector<unsigned char> icon);

    void onContentAvailable() override;

  private:
    GameStat game;
    std::shared_ptr<const PlayStats> stats;
    std::vector<unsigned char> icon_bytes;

    BRLS_BIND(brls::Image,      icon,     "ga_icon");
    BRLS_BIND(brls::Label,      name,     "ga_name");
    BRLS_BIND(PlayDaysView,     days,     "ga_days");
    BRLS_BIND(brls::DetailCell, today,    "ga_today");
    BRLS_BIND(brls::DetailCell, week,     "ga_week");
    BRLS_BIND(brls::DetailCell, total,    "ga_total");
    BRLS_BIND(brls::DetailCell, launches, "ga_launches");
    BRLS_BIND(brls::DetailCell, first,    "ga_first");
    BRLS_BIND(brls::DetailCell, last,     "ga_last");
    BRLS_BIND(brls::Header,     accounts_header, "ga_accounts_header");
    BRLS_BIND(brls::Box,        accounts, "ga_accounts");
    BRLS_BIND(brls::Label,      accounts_note, "ga_accounts_note");
};
