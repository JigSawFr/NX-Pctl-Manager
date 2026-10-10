// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_check_activity.hpp"

#include <fmt/format.h>
#include <string>
#include <vector>

#include "action/history_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
// From this many broken games, the cause is more likely the SD card or the
// storage the console started on than each game.
constexpr size_t MANY = 3;

const char* issue_key(GameIssue issue)
{
    switch (issue) {
        case GC_ARCHIVED:            return "archived";
        case GC_STORAGE_UNAVAILABLE: return "storage";
        case GC_NO_BASE:             return "no_base";
        case GC_FILES_MISSING:       return "files";
        case GC_FIRMWARE_TOO_OLD:    return "firmware";
        case GC_LAUNCH_REFUSED:      return "launch";
        case GC_NO_CONTROL:          return "no_control";
        default:                     return "";
    }
}

std::string hos_text(u32 v)
{
    char buf[16];
    sysinfo_version_string(v, buf, sizeof(buf));
    return buf;
}

std::string game_id(const GameFacts& g)
{
    return fmt::format("{:016X}", (unsigned long long)g.app_id);
}

std::string game_name(const GameFacts& g)
{
    return g.name[0] ? std::string(g.name) : brls::getStr("playguard/games/unnamed", game_id(g));
}

// The cell's value: what the HOME menu shows, in a few words.
std::string short_text(const GameFacts& g, GameIssue issue)
{
    const std::string key = std::string("playguard/games/issues/") + issue_key(issue) + "/short";
    if (issue == GC_FIRMWARE_TOO_OLD) return brls::getStr(key, hos_text(g.required_hos));
    return brls::getStr(key);
}
}   // namespace

void GameCheckActivity::onContentAvailable()
{
    note->setSingleLine(false);
    status->setSingleLine(false);
    hint->setSingleLine(false);
    this->scan();
}

void GameCheckActivity::scan()
{
    if (this->scanning) return;
    this->scanning = true;
    status->setText("playguard/games/scanning"_i18n);
    status->setTextColor(ui::color_text());
    ui::set_visible(status.getView(), true);
    std::weak_ptr<bool> weak = this->alive;
    brls::async([this, weak]() {
        auto check = std::make_shared<GameCheck>();
        gamecheck_scan(check.get());
        brls::sync([this, weak, check]() {
            if (weak.expired()) return;   // the screen was closed meanwhile
            this->scanning = false;
            this->show(*check);
        });
    });
}

void GameCheckActivity::show(const GameCheck& check)
{
    SysInfo si;
    sysinfo_get(&si);
    const u32 hos = si.hos_version;

    struct Broken { GameFacts game; GameIssue issue; };
    std::vector<Broken> broken;
    if (R_SUCCEEDED(check.rc)) {
        for (u32 i = 0; i < check.count; i++) {
            const GameIssue issue = gamecheck_classify(&check.games[i], hos);
            if (issue != GC_OK) broken.push_back({ check.games[i], issue });
        }
    }

    // The cells are reused in place, as in the History screen: one of them
    // may have the focus when this runs again after a removal.
    auto& cells = list->getChildren();
    for (size_t i = 0; i < broken.size(); i++) {
        brls::DetailCell* cell;
        if (i < cells.size()) {
            cell = (brls::DetailCell*)cells[i];
        } else {
            cell = new brls::DetailCell();
            list->addView(cell);
        }
        const Broken b = broken[i];
        cell->setText(game_name(b.game));
        cell->setDetailText(short_text(b.game, b.issue));
        cell->setDetailTextColor(b.issue == GC_STORAGE_UNAVAILABLE || b.issue == GC_FIRMWARE_TOO_OLD ? ui::color_warn()
                                                                                                    : ui::color_bad());
        cell->registerClickAction([this, b, hos](brls::View*) {
            this->open(b.game, b.issue, hos);
            return true;
        });
    }
    for (size_t i = 0; i < broken.size() && i < cells.size(); i++) ui::set_visible(cells[i], true);
    for (size_t i = broken.size(); i < cells.size(); i++) ui::set_visible(cells[i], false);
    // Nothing could have the focus while the games were read.
    if (!this->focused && !broken.empty()) {
        this->focused = true;
        brls::Application::giveFocus(cells[0]);
    }

    std::string text;
    if (R_FAILED(check.rc)) {
        text = "playguard/games/err_list"_i18n + " — " + ui::rc_text(check.rc);
        status->setTextColor(ui::color_bad());
    } else if (broken.empty()) {
        text = brls::getStr("playguard/games/none", (int)check.count);
        status->setTextColor(ui::color_ok());
    } else {
        status->setTextColor(ui::color_text());
    }
    if (check.truncated) text += (text.empty() ? "" : "\n") + brls::getStr("playguard/games/truncated", (int)check.count);
    status->setText(text);
    ui::set_visible(status.getView(), !text.empty());
    ui::set_visible(header.getView(), !broken.empty());
    hint->setText(brls::getStr("playguard/games/many", ui::storage_short(si)));
    ui::set_visible(hint.getView(), broken.size() >= MANY);
}

void GameCheckActivity::open(const GameFacts& game, GameIssue issue, u32 current_hos)
{
    const std::string base = std::string("playguard/games/issues/") + issue_key(issue);
    std::string cause;
    if (issue == GC_FIRMWARE_TOO_OLD) cause = brls::getStr(base + "/cause", hos_text(game.required_hos), hos_text(current_hos));
    else if (issue == GC_LAUNCH_REFUSED) cause = brls::getStr(base + "/cause", ui::rc_text(game.launch_rc));
    else cause = brls::getStr(base + "/cause");

    // An unnamed game is already named by its ID.
    std::string body = game_name(game);
    if (game.name[0]) body += "\n" + brls::getStr("playguard/games/id", game_id(game));
    body += "\n\n" + brls::getStr("playguard/common/line", "playguard/games/cause"_i18n, cause);
    body += "\n\n" + brls::getStr("playguard/common/line", "playguard/games/fix"_i18n, brls::getStr(base + "/fix"));

    if (!gamecheck_removable(issue)) {
        ui::info(body);
        return;
    }
    if (app::read_only()) {
        ui::info(body + "\n\n" + "playguard/common/read_only_note"_i18n);
        return;
    }
    body += "\n\n" + "playguard/games/remove_body"_i18n;
    const u64 id = game.app_id;
    const std::string label = game.name[0] ? fmt::format("{} ({})", game.name, game_id(game)) : game_id(game);
    const std::string name = game_name(game);
    std::weak_ptr<bool> weak = this->alive;
    ui::confirm_danger(body, "playguard/games/remove"_i18n, [this, weak, id, label, name]() {
        const Result rc = gamecheck_remove(id);
        if (R_SUCCEEDED(rc)) history_flow::record_event("remove_game", "", label);
        ui::notify_result(rc, brls::getStr("playguard/games/removed", name), "playguard/games/remove_err"_i18n);
        if (R_SUCCEEDED(rc) && !weak.expired()) this->scan();
    });
}
