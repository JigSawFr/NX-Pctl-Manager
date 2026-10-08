// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_activity.hpp"

#include <fmt/format.h>
#include <string>

#include "ui/ui.hpp"

using namespace brls::literals;

GameActivity::GameActivity(GameStat game, std::shared_ptr<const PlayStats> stats, std::vector<unsigned char> icon)
    : game(game), stats(std::move(stats)), icon_bytes(std::move(icon))
{
}

void GameActivity::onContentAvailable()
{
    accounts_note->setSingleLine(false);
    const std::string na = "playguard/common/unavailable"_i18n;
    const GameStat& g = this->game;

    name->setText(g.name[0] ? std::string(g.name)
                            : brls::getStr("playguard/activity/deleted_game", fmt::format("{:016X}", (unsigned long long)g.app_id)));
    if (!this->icon_bytes.empty()) icon->setImageFromMem(this->icon_bytes.data(), (int)this->icon_bytes.size());
    else ui::set_visible(icon.getView(), false);

    const bool windows = this->stats && this->stats->windows_ok;
    ui::set_visible(days.getView(), windows);
    if (windows) days->show(g.day_s, this->stats->day_wday);
    today->setDetailText(windows ? ui::fmt_play_time(g.today_s) : na);
    week->setDetailText(windows ? ui::fmt_play_time(g.week_s) : na);
    total->setDetailText(g.totals_ok ? ui::fmt_play_time(g.total_s) : na);
    launches->setDetailText(g.totals_ok ? std::to_string(g.launches) : na);
    first->setDetailText(g.totals_ok && g.first_played ? ui::time_text(g.first_played) : "—");
    last->setDetailText(g.totals_ok && g.last_played ? ui::time_text(g.last_played) : "—");

    // All time per user account.
    AccountPlay by_account[PLAYSTATS_MAX_ACCOUNTS];
    Result rc = 0;
    const size_t n = g.totals_ok ? playstats_by_account(g.app_id, by_account, PLAYSTATS_MAX_ACCOUNTS, &rc) : 0;
    for (size_t i = 0; i < n; i++) {
        auto* cell = new brls::DetailCell();
        cell->setFocusable(false);
        cell->setText(by_account[i].nickname[0] ? std::string(by_account[i].nickname) : "?");
        cell->setDetailText(brls::getStr("playguard/activity/account_value", ui::fmt_play_time(by_account[i].total_s),
                                         (int)by_account[i].launches));
        accounts->addView(cell);
    }
    std::string note;
    if (!g.totals_ok) note = "playguard/activity/accounts_none"_i18n;
    else if (n == 0 && R_FAILED(rc)) note = "playguard/activity/err_stats"_i18n + " — " + ui::rc_text(rc);
    else if (n == 0) note = "playguard/activity/accounts_none"_i18n;
    accounts_note->setText(note);
    ui::set_visible(accounts_note.getView(), !note.empty());
}
