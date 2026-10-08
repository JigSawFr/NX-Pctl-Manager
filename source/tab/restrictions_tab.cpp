// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/restrictions_tab.hpp"

#include "action/history_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
const uint8_t AGES[] = { 0, 3, 4, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18 };

std::string age_text(uint8_t age)
{
    return age == 0 ? "playguard/restrictions/age_none"_i18n : brls::getStr("playguard/restrictions/age_value", (int)age);
}

}   // namespace

RestrictionsTab::RestrictionsTab()
    : TabBase("xml/tab/restrictions.xml")
{
    custom_note->setSingleLine(false);
    note->setSingleLine(false);

    level->registerClickAction([this](brls::View*) {
        if (app::read_only()) {
            ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
            return true;
        }
        std::vector<std::string> names;
        for (uint32_t i = 0; i <= 4; i++) names.push_back(ui::level_name(i));
        ui::pick("playguard/restrictions/level"_i18n, names, this->st.safety_level_ok ? (int)this->st.safety_level : 0,
                 [this](int index) {
                     // A preset: say what it restricts (1034) before choosing it.
                     std::string body = brls::getStr("playguard/restrictions/confirm_level", ui::level_name((uint32_t)index));
                     PctlCustomSettings preset;
                     if (index >= PctlSafetyLevel_YoungChild && R_SUCCEEDED(pctl_get_level_settings((uint32_t)index, &preset)))
                         body += "\n\n" + this->settings_text(preset);
                     else if (index == PctlSafetyLevel_Custom)
                         body += "\n\n" + "playguard/restrictions/custom_hint"_i18n;
                     const int before = this->st.safety_level_ok ? (int)this->st.safety_level : -1;
                     ui::confirm(body, "playguard/play_timer/confirm_set"_i18n, [this, index, before]() {
                                     Result rc = pctl_set_safety_level((uint32_t)index);
                                     if (R_SUCCEEDED(rc) && before >= 0) history_flow::record_values("level", { before }, { index });
                                     ui::notify_result(rc, "playguard/restrictions/saved"_i18n, "playguard/restrictions/save_err"_i18n);
                                     this->refresh();
                                 });
                 });
        return true;
    });

    age->registerClickAction([this](brls::View*) {
        if (!this->st.settings_ok) return true;
        if (app::read_only()) {
            ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
            return true;
        }
        std::vector<std::string> names;
        int selected = 0;
        for (size_t i = 0; i < sizeof(AGES); i++) {
            names.push_back(age_text(AGES[i]));
            if (AGES[i] == this->st.settings.rating_age) selected = (int)i;
        }
        ui::pick("playguard/restrictions/age"_i18n, names, selected, [this](int index) {
            PctlCustomSettings s = this->st.settings;
            s.rating_age = AGES[index];
            this->write_custom(s);
        });
        return true;
    });

    sns->init("playguard/restrictions/sns"_i18n, false, [this](bool on) {
        PctlCustomSettings s = this->st.settings;
        s.sns_post_restriction = on;
        this->write_custom(s);
    });
    comm->init("playguard/restrictions/comm"_i18n, false, [this](bool on) {
        PctlCustomSettings s = this->st.settings;
        s.free_communication_restriction = on;
        this->write_custom(s);
    });
    org->registerClickAction([this](brls::View*) {
        if (app::read_only()) {
            ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
            return true;
        }
        std::vector<std::string> names;
        for (uint32_t i = 0; i < 13; i++) names.push_back(pctl_rating_org_name(i));
        ui::pick("playguard/restrictions/org"_i18n, names, this->st.rating_org_ok && this->st.rating_org < 13 ? (int)this->st.rating_org : 0,
                 [this](int index) {
                     if (this->st.rating_org_ok && (int)this->st.rating_org == index) return;
                     ui::confirm(brls::getStr("playguard/restrictions/confirm_org", pctl_rating_org_name((uint32_t)index)),
                                 "playguard/play_timer/confirm_set"_i18n, [this, index]() {
                                     Result rc = pctl_set_rating_org((uint32_t)index);
                                     if (R_SUCCEEDED(rc) && this->st.rating_org_ok)
                                         history_flow::record_values("org", { (int)this->st.rating_org }, { index });
                                     ui::notify_result(rc, "playguard/restrictions/saved"_i18n, "playguard/restrictions/save_err"_i18n);
                                     this->refresh();
                                 });
                 });
        return true;
    });

    vr->init("playguard/restrictions/vr"_i18n, false, [this](bool on) {
        Result rc = pctl_set_stereo_vision_restricted(on);
        if (R_SUCCEEDED(rc)) history_flow::record_values("vr", { on ? 0 : 1 }, { on ? 1 : 0 });
        ui::notify_result(rc, "playguard/restrictions/saved"_i18n, "playguard/restrictions/save_err"_i18n);
        this->refresh();
    });
    ui::guard_switch(sns);
    ui::guard_switch(comm);
    ui::guard_switch(vr);
}

std::string RestrictionsTab::settings_text(const PctlCustomSettings& s) const
{
    const std::string org = this->st.rating_org_ok ? std::string(" (") + pctl_rating_org_name(this->st.rating_org) + ")" : "";
    auto yes_no = [](bool v) { return v ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n; };
    return brls::getStr("playguard/common/line", "playguard/restrictions/age"_i18n, age_text(s.rating_age) + org) + "\n" +
           brls::getStr("playguard/common/line", "playguard/restrictions/sns"_i18n, yes_no(s.sns_post_restriction)) + "\n" +
           brls::getStr("playguard/common/line", "playguard/restrictions/comm"_i18n, yes_no(s.free_communication_restriction));
}

void RestrictionsTab::write_custom(const PctlCustomSettings& s)
{
    Result rc = pctl_set_custom_settings(&s);
    if (R_SUCCEEDED(rc) && this->st.settings_ok)
        history_flow::record_values("custom", history_flow::custom_values(this->st.settings), history_flow::custom_values(s));
    ui::notify_result(rc, "playguard/restrictions/saved"_i18n, "playguard/restrictions/save_err"_i18n);
    this->refresh();   // re-reads, so a refused change flips the switch back
}

void RestrictionsTab::refresh()
{
    pctl_status_fetch(&this->st);
    ui::note_unlocked(this->st.temp_unlocked_ok, this->st.temp_unlocked);
    const std::string na = "playguard/common/unavailable"_i18n;

    level->setDetailText(this->st.safety_level_ok ? ui::level_name(this->st.safety_level) : na);

    const bool custom = this->st.safety_level_ok && this->st.safety_level == PctlSafetyLevel_Custom &&
                        this->st.settings_ok;
    // A preset: what it imposes, read-only, instead of a note saying the
    // settings are hidden (the parent sees what applies without the picker).
    PctlCustomSettings preset = {};
    const bool preset_ok = !custom && this->st.safety_level_ok && this->st.safety_level != PctlSafetyLevel_Custom &&
                           R_SUCCEEDED(pctl_get_level_settings(this->st.safety_level, &preset));
    auto yes_no = [](bool v) { return v ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n; };
    custom_header->setTitle(custom ? "playguard/restrictions/section_custom"_i18n
                                   : "playguard/restrictions/section_level_settings"_i18n);
    ui::set_visible(custom_header.getView(), custom || preset_ok);
    ui::set_visible(preset_age.getView(), preset_ok);
    ui::set_visible(preset_sns.getView(), preset_ok);
    ui::set_visible(preset_comm.getView(), preset_ok);
    if (preset_ok) {
        preset_age->setDetailText(age_text(preset.rating_age));
        preset_sns->setDetailText(yes_no(preset.sns_post_restriction));
        preset_comm->setDetailText(yes_no(preset.free_communication_restriction));
    }
    ui::set_visible(age.getView(), custom);
    ui::set_visible(sns.getView(), custom);
    ui::set_visible(comm.getView(), custom);
    ui::set_visible(custom_note.getView(), !custom);
    if (this->st.settings_ok) {
        age->setDetailText(age_text(this->st.settings.rating_age));
        sns->setOn(this->st.settings.sns_post_restriction, false);
        comm->setOn(this->st.settings.free_communication_restriction, false);
    }

    ui::set_visible(vr.getView(), this->st.stereo_vision_ok);
    if (this->st.stereo_vision_ok) vr->setOn(this->st.stereo_vision_restricted, false);

    org->setDetailText(this->st.rating_org_ok ? pctl_rating_org_name(this->st.rating_org) : na);
    // Read-only: everything stays in sight, greyed; A says why nothing changes.
    const bool writable = !app::read_only();
    for (brls::DetailCell* c : { (brls::DetailCell*)level.getView(), (brls::DetailCell*)age.getView(),
                                 (brls::DetailCell*)sns.getView(), (brls::DetailCell*)comm.getView(),
                                 (brls::DetailCell*)vr.getView(), (brls::DetailCell*)org.getView() })
        ui::show_writable(c, writable);
    free_comm->setDetailText(this->st.free_comm_count_ok
        ? brls::getStr("playguard/restrictions/free_comm_value", (int)this->st.free_comm_count) : na);
}

brls::View* RestrictionsTab::create()
{
    return new RestrictionsTab();
}
