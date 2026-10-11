// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pin_lock.hpp"

#include <borealis.hpp>
#include <chrono>
#include <vector>

#include "action/pin_lock_logic.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace pin_lock
{

namespace
{
using pin_lock_logic::MODES;
using pin_lock_logic::rank;

std::chrono::steady_clock::time_point s_confirmed_until, s_refused_until;
std::chrono::steady_clock::time_point s_pin_at;        // when the PIN was last entered
std::chrono::steady_clock::time_point s_reveal_until;  // before_show_pin() just asked
std::chrono::steady_clock::time_point s_away_since;    // out of focus since (s_away)
bool s_away = false;
Result s_last_rc = 0;   // what the last ask() got

// The service layer's change check (write_guard.h). Runs on the UI thread,
// with no pctl session open.
bool check()
{
    using pin_lock_logic::Check;
    const Check c = pin_lock_logic::check(config::get().pin_lock, std::chrono::steady_clock::now(),
                                          s_confirmed_until, s_refused_until);
    if (c == Check::Allow) return true;
    if (c == Check::Refuse) return false;
    if (ask()) return true;
    // Held from the end of the PIN screen, not from when it opened.
    s_refused_until = std::chrono::steady_clock::now() + pin_lock_logic::REFUSAL_HOLDS;
    return false;
}

// The service layer's reveal check: the PIN every time, whatever the mode,
// the grace or the lock screen let through. Whoever may see the PIN knows it.
bool reveal_check()
{
    const auto now = std::chrono::steady_clock::now();
    if (now < s_reveal_until) {   // asked by before_show_pin() a moment ago: once
        s_reveal_until = {};
        return true;
    }
    return ask();
}
}   // namespace

void install()
{
    core_set_change_check(check);
    core_set_reveal_check(reveal_check);
}

void watch_focus(std::function<void()> lock)
{
    brls::Application::getWindowFocusChangedEvent()->subscribe([lock](bool focused) {
        const auto now = std::chrono::steady_clock::now();
        if (!focused) {
            if (!s_away) s_away_since = now;
            s_away = true;
            return;
        }
        if (!s_away) return;
        s_away = false;
        const auto since = s_away_since;
        // After this frame: a PIN screen that took the focus has returned by then.
        brls::sync([lock, since, now]() {
            if (pin_lock_logic::lock_again(config::get().pin_lock, since, now, s_pin_at)) lock();
        });
    });
}

bool at_start()
{
    return config::get().pin_lock == "open";
}

bool ask()
{
    const Result rc = pctl_ask_pin();
    s_last_rc = rc;
    brls::Logger::info("pctl_ask_pin returned 0x{:08X}", (unsigned)rc);
    if (R_SUCCEEDED(rc)) {
        s_pin_at = std::chrono::steady_clock::now();
        s_confirmed_until = s_pin_at + pin_lock_logic::GRACE;
        return true;
    }
    return rc == NXM_RC_NO_PIN;   // nothing to ask for
}

bool before_show_pin()
{
    if (!ask()) {
        ui::notify(refusal_text());
        return false;
    }
    // The read that follows at once does not ask a second time.
    s_reveal_until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    return true;
}

std::string refusal_text()
{
    if (R_SUCCEEDED(s_last_rc) || NXM_IS_APP_RESULT(s_last_rc)) return ui::rc_text(NXM_RC_NOT_CONFIRMED);
    return brls::getStr("playguard/error/code_hint", fmt::format("0x{:08X}", (unsigned)s_last_rc),
                        "playguard/error/not_confirmed"_i18n);
}

std::string mode_text()
{
    return brls::getStr(std::string("playguard/pin_lock/modes/") + MODES[rank(config::get().pin_lock)]);
}

std::string note_text()
{
    return brls::getStr(std::string("playguard/pin_lock/notes/") + MODES[rank(config::get().pin_lock)]);
}

void choose(std::function<void()> done)
{
    std::vector<std::string> labels;
    for (const char* m : MODES) labels.push_back(brls::getStr(std::string("playguard/pin_lock/modes/") + m));
    ui::pick("playguard/pin_lock/title"_i18n, labels, rank(config::get().pin_lock), [done, labels](int index) {
        const std::string from = config::get().pin_lock, to = MODES[index];
        if (to == from) return;
        PctlStatus st;
        pctl_status_fetch(&st);
        const bool has_pin = st.pin_length_ok && st.pin_length > 0;
        const pin_lock_logic::Change change = pin_lock_logic::change(from, index, has_pin);
        if (change == pin_lock_logic::Change::NoPin) {
            ui::notify("playguard/pin_lock/no_pin"_i18n);
            return;
        }
        if (change == pin_lock_logic::Change::AskPin && !ask()) {
            ui::notify(refusal_text());
            return;
        }
        config::get().pin_lock = to;
        if (ui::save_config()) ui::notify(brls::getStr("playguard/pin_lock/set", labels[index]));
        if (done) done();
    });
}

}   // namespace pin_lock
