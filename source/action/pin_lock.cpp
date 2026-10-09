// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pin_lock.hpp"

#include <borealis.hpp>
#include <chrono>
#include <vector>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace pin_lock
{

namespace
{
constexpr auto GRACE = std::chrono::minutes(5);
// One action can make several changes (a restore): after a refusal, the
// next ones are refused too instead of asking once per change.
constexpr auto REFUSAL_HOLDS = std::chrono::seconds(3);
std::chrono::steady_clock::time_point s_confirmed_until, s_refused_until;
Result s_last_rc = 0;   // what the last ask() got
const char* MODES[] = { "off", "changes", "open" };

int rank(const std::string& mode)
{
    for (int i = 0; i < 3; i++)
        if (mode == MODES[i]) return i;
    return 0;
}

// The service layer's change check (write_guard.h). Runs on the UI thread,
// with no pctl session open.
bool check()
{
    if (config::get().pin_lock != "changes") return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < s_confirmed_until) return true;
    if (now < s_refused_until) return false;
    if (ask()) return true;
    s_refused_until = std::chrono::steady_clock::now() + REFUSAL_HOLDS;
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
        s_confirmed_until = std::chrono::steady_clock::now() + GRACE;
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
        if (index > 0 && !has_pin) {   // nothing to ask for: it would protect nothing
            ui::notify("playguard/pin_lock/no_pin"_i18n);
            return;
        }
        // Less protection than now: only the parent may choose it.
        if (index < rank(from) && has_pin && !ask()) {
            ui::notify(refusal_text());
            return;
        }
        config::get().pin_lock = to;
        if (ui::save_config()) ui::notify(brls::getStr("playguard/pin_lock/set", labels[index]));
        if (done) done();
    });
}

}   // namespace pin_lock
