// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/history_flow.hpp"

#include <borealis.hpp>
#include <fmt/format.h>

#include "action/data_notice.hpp"
#include "action/history_logic.hpp"
#include "action/outside_watch.hpp"
#include "action/pt_flow.hpp"
#include "action/pt_log_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace history_flow
{

namespace
{
std::string yes_no(int v)
{
    return v ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n;
}

std::string age_text(int age)
{
    return age == 0 ? "playguard/restrictions/age_none"_i18n : brls::getStr("playguard/restrictions/age_value", age);
}

// One recorded value set, worded for its kind ("" when it does not fit).
std::string value_text(const std::string& kind, const std::vector<int>& v)
{
    if ((kind == "limits" || kind == "outside_limits") && v.size() == 7) {
        uint16_t days[7];
        for (int i = 0; i < 7; i++) days[i] = (uint16_t)v[i];
        return ui::days_summary(days);
    }
    if (v.size() == 1) {
        if (kind == "level") return ui::level_name((uint32_t)v[0]);
        if (kind == "org") return pctl_rating_org_name((uint32_t)v[0]);
        if (kind == "vr") return yes_no(v[0]);
        if (kind == "alarm") return v[0] ? "playguard/common/off"_i18n : "playguard/common/on"_i18n;
        if (kind == "console_lock") return v[0] ? "playguard/common/on"_i18n : "playguard/common/off"_i18n;
    }
    if (kind == "custom" && v.size() == 3)
        return brls::getStr("playguard/history/custom_value", age_text(v[0]), yes_no(v[1]), yes_no(v[2]));
    return "";
}

std::string kind_label(const std::string& kind)
{
    static const char* known[] = { "limits", "level", "custom", "org", "vr", "alarm", "pin", "pin_shown",
                                   "unlock", "relock", "unlink", "delete", "clock", "restore", "rescue",
                                   "console_lock", "bedtime", "config_reset", "outside_reset", "outside_clock",
                                   "outside_limits" };
    for (const char* k : known)
        if (kind == k) return brls::getStr(std::string("playguard/history/kinds/") + k);
    return kind;
}

std::string source_label(const history::Entry& e)
{
    static const char* known[] = { "uniform", "day", "per_day", "extra", "stop", "restore_extra",
                                   "remove", "backup", "undo", "first_steps", "overview", "console_lock" };
    if (e.source == "profile") return brls::getStr("playguard/history/sources/profile", e.detail);
    for (const char* s : known)
        if (e.source == s) return brls::getStr(std::string("playguard/history/sources/") + s);
    return "";
}

// The console's value of `kind` now, as recorded values ({} when unread).
std::vector<int> current(const std::string& kind)
{
    if (kind == "limits" || kind == "alarm") {
        PtState pt;
        pctl_play_timer_query(&pt);
        if (kind == "limits") return pt.valid ? days_values(pt.day_min) : std::vector<int>{};
        return pt.alarm_disabled_valid ? std::vector<int>{ pt.alarm_disabled ? 1 : 0 } : std::vector<int>{};
    }
    PctlStatus st;
    pctl_status_fetch(&st);
    if (kind == "level" && st.safety_level_ok) return { (int)st.safety_level };
    if (kind == "org" && st.rating_org_ok) return { (int)st.rating_org };
    if (kind == "vr" && st.stereo_vision_ok) return { st.stereo_vision_restricted ? 1 : 0 };
    if (kind == "custom" && st.settings_ok) return custom_values(st.settings);
    return {};
}

// Writes `v` for `kind` (not the limits or the alarm: they go through the
// play-timer gate).
Result write_value(const std::string& kind, const std::vector<int>& v)
{
    if (kind == "level") return pctl_set_safety_level((uint32_t)v[0]);
    if (kind == "org") return pctl_set_rating_org((uint32_t)v[0]);
    if (kind == "vr") return pctl_set_stereo_vision_restricted(v[0] != 0);
    if (kind == "custom") {
        PctlCustomSettings s = { (uint8_t)v[0], v[1] != 0, v[2] != 0 };
        return pctl_set_custom_settings(&s);
    }
    return NXM_RC_INVALID_ARGUMENT;
}
}   // namespace

std::vector<int> days_values(const uint16_t days[7])
{
    return std::vector<int>(days, days + 7);
}

std::vector<int> custom_values(const PctlCustomSettings& s)
{
    return { s.rating_age, s.sns_post_restriction ? 1 : 0, s.free_communication_restriction ? 1 : 0 };
}

static void store(history::Entry e)
{
    // A change PlayGuard made: what it reads next is not a change made
    // outside (the outside_* entries are those).
    if (e.kind.rfind("outside_", 0) != 0) outside_watch::own_change();
    e.when = ui::now_stamp();
    std::string err;
    bool put_aside = false;
    if (!history::append(e, &err, &put_aside)) brls::Logger::warning("history: not saved ({})", err);
    if (put_aside) data_notice::history_put_aside();
    // Developer › Record the play timer: a line marking the change.
    std::string event = e.kind;
    for (const std::string* part : { &e.source, &e.detail })
        if (!part->empty()) event += " " + *part;
    if (!e.before.empty() || !e.after.empty()) {
        auto list = [](const std::vector<int>& v) {
            std::string out;
            for (int x : v) out += (out.empty() ? "" : " ") + std::to_string(x);
            return out;
        };
        event += " [" + list(e.before) + "] -> [" + list(e.after) + "]";
    }
    pt_log_flow::note(event);
}

void record_values(const char* kind, std::vector<int> before, std::vector<int> after,
                   const std::string& source, const std::string& detail)
{
    if (before == after) return;
    history::Entry e;
    e.kind   = kind;
    e.source = source;
    e.detail = detail;
    e.before = std::move(before);
    e.after  = std::move(after);
    store(e);
}

void record_event(const char* kind, const std::string& source, const std::string& detail)
{
    history::Entry e;
    e.kind   = kind;
    e.source = source;
    e.detail = detail;
    store(e);
}

std::string title(const history::Entry& e)
{
    const std::string before = value_text(e.kind, e.before), after = value_text(e.kind, e.after);
    if (!before.empty() && !after.empty())
        return brls::getStr("playguard/history/title_change", kind_label(e.kind), before, after);
    if (!e.detail.empty() && e.source.empty()) return brls::getStr("playguard/common/line", kind_label(e.kind), e.detail);
    return kind_label(e.kind);
}

std::string details(const history::Entry& e)
{
    std::string out = kind_label(e.kind) + "\n";
    out += "\n" + brls::getStr("playguard/common/line", "playguard/history/when"_i18n, e.when.empty() ? "?" : e.when);
    const std::string from = source_label(e);
    if (!from.empty()) out += "\n" + brls::getStr("playguard/common/line", "playguard/history/how"_i18n, from);
    else if (!e.detail.empty()) out += "\n" + e.detail;
    const std::string before = value_text(e.kind, e.before), after = value_text(e.kind, e.after);
    if (!before.empty()) out += "\n" + brls::getStr("playguard/common/line", "playguard/history/before"_i18n, before);
    if (!after.empty()) out += "\n" + brls::getStr("playguard/common/line", "playguard/history/after"_i18n, after);
    return out;
}

void open(const history::Entry& e, std::function<void()> refresh)
{
    std::string body = details(e);
    if (!history::undoable(e)) {
        ui::info(body);
        return;
    }
    const std::vector<int> now = current(e.kind);
    using history_logic::Undo;
    switch (history_logic::undo_action(e, now, app::read_only(), config::get().advanced)) {
    case Undo::AlreadyBack:
        ui::info(body + "\n\n" + "playguard/history/already_back"_i18n);
        return;
    // Nothing to offer: the details alone, with the reason.
    case Undo::ReadOnly:
        ui::info(body + "\n\n" + "playguard/common/read_only_note"_i18n);
        return;
    case Undo::NeedsAdvanced:
        ui::info(body + "\n\n" + "playguard/history/needs_advanced"_i18n);
        return;
    case Undo::Offer:
        break;
    }
    if (history_logic::changed_since(e, now))
        body += "\n\n" + brls::getStr("playguard/history/changed_since", value_text(e.kind, now));
    body += "\n\n" + brls::getStr("playguard/history/undo_body", value_text(e.kind, e.before));

    if (e.kind == "limits") {
        // Through the play-timer gate, with the week as it will be drawn.
        uint16_t days[7];
        for (int i = 0; i < 7; i++) days[i] = (uint16_t)e.before[i];
        const std::vector<int> before = e.before;
        pt_flow::confirm_write(body, "playguard/history/undo_confirm"_i18n, [before, refresh](bool did_unlock) {
            uint16_t back[7];
            for (int i = 0; i < 7; i++) back[i] = (uint16_t)before[i];
            const Result rc = pt_flow::write_days(back, "undo");
            pt_flow::finish_write(rc, did_unlock, "playguard/history/undone"_i18n, "playguard/history/undo_err"_i18n, refresh);
        }, days);
        return;
    }
    if (e.kind == "alarm") {
        // A play-timer write too: through the gate.
        const bool disabled = e.before[0] != 0;
        pt_flow::confirm_write(body, "playguard/history/undo_confirm"_i18n, [disabled, refresh](bool did_unlock) {
            const Result rc = pt_flow::write_alarm_disabled(disabled, "undo");
            pt_flow::finish_write(rc, did_unlock, "playguard/history/undone"_i18n, "playguard/history/undo_err"_i18n, refresh);
        });
        return;
    }
    const history::Entry entry = e;
    ui::confirm(body, "playguard/history/undo_confirm"_i18n, [entry, now, refresh]() {
        const Result rc = write_value(entry.kind, entry.before);
        if (R_SUCCEEDED(rc)) record_values(entry.kind.c_str(), history_logic::undo_replaces(entry, now), entry.before, "undo");
        ui::notify_result(rc, "playguard/history/undone"_i18n, "playguard/history/undo_err"_i18n);
        if (refresh) refresh();
    });
}

}   // namespace history_flow
