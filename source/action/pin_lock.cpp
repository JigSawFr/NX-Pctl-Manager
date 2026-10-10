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
}   // namespace

void install()
{
    core_set_change_check(check);
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
        s_confirmed_until = std::chrono::steady_clock::now() + pin_lock_logic::GRACE;
        return true;
    }
    return rc == NXM_RC_NO_PIN;   // nothing to ask for
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
