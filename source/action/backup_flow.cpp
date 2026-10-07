// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/backup_flow.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <memory>
#include <vector>

#include "action/pt_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace backup_flow
{

namespace
{
std::string line(const std::string& label, const std::string& value)
{
    return brls::getStr("playguard/common/line", label, value);
}

std::string yes_no(bool value)
{
    return value ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n;
}

std::string age_text(uint8_t age)
{
    return age == 0 ? "playguard/restrictions/age_none"_i18n : brls::getStr("playguard/restrictions/age_value", (int)age);
}

// "2 h every day", or "Mon 2 h · Tue 2 h · …" (Monday first).
std::string days_text(const std::array<uint16_t, 7>& days)
{
    bool uniform = true;
    for (int d = 1; d < 7; d++) uniform &= days[d] == days[0];
    if (uniform)
        return days[0] == PT_DAY_NOLIMIT ? "playguard/common/no_limit"_i18n
                                         : brls::getStr("playguard/play_timer/state/every_day", ui::fmt_minutes(days[0]));
    std::string out;
    for (int i = 1; i <= 7; i++) {
        const int d = i % 7;
        if (!out.empty()) out += " · ";
        out += brls::getStr(fmt::format("playguard/days_short/{}", d)) + " " + ui::fmt_minutes(days[d]);
    }
    return out;
}

// What a restore will write, one line per value.
std::string summary(const backup::Snapshot& s)
{
    std::string out = brls::getStr("playguard/backup/restore_body", s.created.empty() ? "?" : s.created) + "\n";
    if (s.level_ok) out += "\n" + line("playguard/restrictions/section_level"_i18n, ui::level_name(s.level));
    if (s.level_ok && s.level == PctlSafetyLevel_Custom && s.custom_ok) {
        out += "\n" + line("playguard/restrictions/age"_i18n, age_text(s.rating_age));
        out += "\n" + line("playguard/restrictions/sns"_i18n, yes_no(s.sns_restricted));
        out += "\n" + line("playguard/restrictions/comm"_i18n, yes_no(s.comm_restricted));
    }
    if (s.vr_ok) out += "\n" + line("playguard/restrictions/vr"_i18n, yes_no(s.vr_restricted));
    if (s.days_ok) out += "\n" + line("playguard/play_timer/section_limit"_i18n, days_text(s.days));
    // The PIN is not in the backup (after "Delete all", there is none).
    PctlStatus st;
    pctl_status_fetch(&st);
    if (st.pin_length_ok && st.pin_length == 0) out += "\n\n" + "playguard/backup/no_pin_note"_i18n;
    return out;
}

// Writes every value of the backup, in the order the service needs (the level
// before the custom settings), and goes on after a failure so that one value
// the console refuses does not leave the others unrestored.
void write_all(const backup::Snapshot& s, bool did_unlock, std::function<void()> refresh)
{
    Result first = 0;
    std::vector<std::string> failed;
    auto check = [&](Result rc, const std::string& what) {
        if (R_SUCCEEDED(rc)) return;
        if (R_SUCCEEDED(first)) first = rc;
        failed.push_back(what);
    };
    if (s.level_ok) {
        check(pctl_set_safety_level(s.level), "playguard/restrictions/section_level"_i18n);
        if (s.level == PctlSafetyLevel_Custom && s.custom_ok) {
            PctlCustomSettings cs = { s.rating_age, s.sns_restricted, s.comm_restricted };
            check(pctl_set_custom_settings(&cs), "playguard/restrictions/section_custom"_i18n);
        }
    }
    if (s.vr_ok) check(pctl_set_stereo_vision_restricted(s.vr_restricted), "playguard/restrictions/vr"_i18n);
    if (s.days_ok) check(pctl_play_timer_set_days(s.days.data()), "playguard/play_timer/section_limit"_i18n);

    std::string what;
    for (const auto& f : failed) what += (what.empty() ? "" : ", ") + f;
    pt_flow::finish_write(first, did_unlock, "playguard/backup/restored"_i18n,
                          brls::getStr("playguard/backup/restore_err", what), refresh);
}

void restore(const backup::Snapshot& s, std::function<void()> refresh)
{
    const std::string body = summary(s);
    auto run = [s, refresh](bool did_unlock) { write_all(s, did_unlock, refresh); };
    // Limits to write: through the play-timer gate (unlock first when the
    // timer is counting down). Restrictions only: a plain confirmation, as in
    // the Restrictions tab.
    // It overwrites the current settings: the confirm button says so by its colour.
    if (s.days_ok) pt_flow::confirm_write(body, "playguard/backup/restore_confirm"_i18n, run, s.days.data(), true);
    else if (app::read_only()) ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
    else ui::confirm(body, "playguard/backup/restore_confirm"_i18n, [run]() { run(false); }, nullptr, true);
}

int s_count = -1;   // backups on the SD card, -1: not listed yet

std::string save_snapshot(std::string* error)
{
    s_count = -1;   // list again next time: one more file, or a failed save
    const backup::Snapshot s = capture();
    if (s.empty()) {
        *error = "playguard/backup/unreadable_console"_i18n;
        return "";
    }
    const std::string path = backup::save(s, error);
    // Tools › Keep backups: the oldest beyond that number go (never the new one).
    if (!path.empty()) backup::prune((size_t)config::get().backup_keep);
    return path;
}
}   // namespace

size_t count()
{
    if (s_count < 0) s_count = (int)backup::list().size();
    return (size_t)s_count;
}

backup::Snapshot capture()
{
    backup::Snapshot s;

    PctlStatus st;
    pctl_status_fetch(&st);
    s.level_ok        = st.safety_level_ok && st.safety_level <= PctlSafetyLevel_Teen;
    s.level           = st.safety_level;
    s.custom_ok       = st.settings_ok && st.settings.rating_age <= 21;
    s.rating_age      = st.settings.rating_age;
    s.sns_restricted  = st.settings.sns_post_restriction;
    s.comm_restricted = st.settings.free_communication_restriction;
    s.vr_ok           = st.stereo_vision_ok;
    s.vr_restricted   = st.stereo_vision_restricted;

    PtState pt;
    pctl_play_timer_query(&pt);
    s.days_ok = pt.fw_supported && pt.valid;
    for (int d = 0; d < 7 && s.days_ok; d++) {
        s.days[d] = pt.day_min[d];
        if (s.days[d] != PT_DAY_NOLIMIT && s.days[d] > 1440) s.days_ok = false;
    }

    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    s.firmware = fw;

    s.created = ui::now_stamp();
    return s;
}

void save_now()
{
    std::string err;
    const std::string path = save_snapshot(&err);
    if (path.empty()) ui::notify("playguard/backup/save_err"_i18n + ": " + err);
    else ui::notify(brls::getStr("playguard/backup/saved", path));
}

void choose_and_restore(std::function<void()> refresh)
{
    const auto names = backup::list();
    if (names.empty()) {
        ui::notify("playguard/backup/none"_i18n);
        return;
    }
    // Loaded once: the restore uses exactly what the list showed.
    auto loaded = std::make_shared<std::vector<std::pair<bool, backup::Snapshot>>>();
    std::vector<std::string> labels;
    for (const auto& name : names) {
        backup::Snapshot s;
        const bool ok = backup::load(name, s);
        loaded->emplace_back(ok, s);
        if (!ok) labels.push_back(brls::getStr("playguard/backup/unreadable", name));
        else if (s.created.empty()) labels.push_back(name);
        else labels.push_back(s.firmware.empty() ? s.created : s.created + " · " + s.firmware);
    }
    ui::pick("playguard/backup/pick_title"_i18n, labels, 0, [loaded, refresh](int index) {
        const auto& entry = (*loaded)[(size_t)index];
        if (!entry.first) ui::notify("playguard/backup/unreadable_file"_i18n);
        else restore(entry.second, refresh);
    });
}

void backup_then(std::function<void()> next)
{
    std::string err;
    const std::string path = save_snapshot(&err);
    if (!path.empty()) {
        ui::notify(brls::getStr("playguard/backup/saved", path));
        next();
        return;
    }
    brls::sync([err, next]() {
        ui::confirm_danger(brls::getStr("playguard/backup/before_delete_err", err),
                           "playguard/backup/delete_anyway"_i18n, next);
    });
}

}   // namespace backup_flow
