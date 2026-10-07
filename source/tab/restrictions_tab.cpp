// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/restrictions_tab.hpp"

#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
const uint8_t AGES[] = { 0, 3, 4, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18 };

std::string age_text(uint8_t age)
{
    return age == 0 ? "nx_pctl/restrictions/age_none"_i18n : brls::getStr("nx_pctl/restrictions/age_value", (int)age);
}
}   // namespace

RestrictionsTab::RestrictionsTab()
    : TabBase("xml/tab/restrictions.xml")
{
    custom_note->setSingleLine(false);
    note->setSingleLine(false);

    level->registerClickAction([this](brls::View*) {
        std::vector<std::string> names;
        for (uint32_t i = 0; i <= 4; i++) names.push_back(ui::level_name(i));
        ui::pick("nx_pctl/restrictions/level"_i18n, names, this->st.safety_level_ok ? (int)this->st.safety_level : 0,
                 [this](int index) {
                     ui::confirm(brls::getStr("nx_pctl/restrictions/confirm_level", ui::level_name((uint32_t)index)),
                                 "nx_pctl/play_timer/confirm_set"_i18n, [this, index]() {
                                     Result rc = pctl_set_safety_level((uint32_t)index);
                                     ui::notify_result(rc, "nx_pctl/restrictions/saved"_i18n, "nx_pctl/restrictions/save_err"_i18n);
                                     this->refresh();
                                 });
                 });
        return true;
    });

    age->registerClickAction([this](brls::View*) {
        if (!this->st.settings_ok) return true;
        std::vector<std::string> names;
        int selected = 0;
        for (size_t i = 0; i < sizeof(AGES); i++) {
            names.push_back(age_text(AGES[i]));
            if (AGES[i] == this->st.settings.rating_age) selected = (int)i;
        }
        ui::pick("nx_pctl/restrictions/age"_i18n, names, selected, [this](int index) {
            PctlCustomSettings s = this->st.settings;
            s.rating_age = AGES[index];
            this->write_custom(s);
        });
        return true;
    });

    sns->init("nx_pctl/restrictions/sns"_i18n, false, [this](bool on) {
        PctlCustomSettings s = this->st.settings;
        s.sns_post_restriction = on;
        this->write_custom(s);
    });
    comm->init("nx_pctl/restrictions/comm"_i18n, false, [this](bool on) {
        PctlCustomSettings s = this->st.settings;
        s.free_communication_restriction = on;
        this->write_custom(s);
    });
    vr->init("nx_pctl/restrictions/vr"_i18n, false, [this](bool on) {
        Result rc = pctl_set_stereo_vision_restricted(on);
        ui::notify_result(rc, "nx_pctl/restrictions/saved"_i18n, "nx_pctl/restrictions/save_err"_i18n);
        this->refresh();
    });
}

void RestrictionsTab::write_custom(const PctlCustomSettings& s)
{
    Result rc = pctl_set_custom_settings(&s);
    ui::notify_result(rc, "nx_pctl/restrictions/saved"_i18n, "nx_pctl/restrictions/save_err"_i18n);
    this->refresh();   // re-reads, so a refused change flips the switch back
}

void RestrictionsTab::refresh()
{
    pctl_status_fetch(&this->st);
    const std::string na = "nx_pctl/common/unavailable"_i18n;

    level->setDetailText(this->st.safety_level_ok ? ui::level_name(this->st.safety_level) : na);

    const bool custom = this->st.safety_level_ok && this->st.safety_level == PctlSafetyLevel_Custom &&
                        this->st.settings_ok;
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
    free_comm->setDetailText(this->st.free_comm_count_ok
        ? brls::getStr("nx_pctl/restrictions/free_comm_value", (int)this->st.free_comm_count) : na);
}

brls::View* RestrictionsTab::create()
{
    return new RestrictionsTab();
}
