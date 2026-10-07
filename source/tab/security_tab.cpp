// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/security_tab.hpp"

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

SecurityTab::SecurityTab()
    : TabBase("xml/tab/security.xml")
{
    set_pin->registerClickAction([this](brls::View*) {
        // Blocks while the system PIN applet is shown; the session is released first.
        Result rc = pctl_set_pin();
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        ui::notify_result(rc, "nx_pctl/security/pin_ok"_i18n, "nx_pctl/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    unlock->registerClickAction([this](brls::View*) {
        ui::confirm("nx_pctl/security/unlock_body"_i18n, "nx_pctl/security/unlock_confirm"_i18n, [this]() {
            Result rc = pctl_unlock_restriction_temporarily();
            ui::notify_result(rc, "nx_pctl/security/unlocked"_i18n, "nx_pctl/security/unlock_err"_i18n);
            this->refresh();
        });
        return true;
    });
    relock->registerClickAction([this](brls::View*) {
        Result rc = pctl_relock();
        ui::notify_result(rc, "nx_pctl/toast/relocked"_i18n, "nx_pctl/toast/relock_err"_i18n);
        this->refresh();
        return true;
    });
    del->registerClickAction([this](brls::View*) {
        ui::confirm("nx_pctl/security/delete_body"_i18n, "nx_pctl/security/delete_confirm"_i18n, [this]() {
            // Second, separate confirmation: this cannot be undone.
            brls::sync([this]() {
                ui::confirm("nx_pctl/security/delete_body2"_i18n, "nx_pctl/security/delete_confirm"_i18n, [this]() {
                    Result rc = pctl_delete_parental_controls();
                    ui::notify_result(rc, "nx_pctl/security/deleted"_i18n, "nx_pctl/security/delete_err"_i18n);
                    this->refresh();
                });
            });
        });
        return true;
    });
}

void SecurityTab::refresh()
{
    PctlStatus s;
    pctl_status_fetch(&s);
    const std::string na = "nx_pctl/common/unavailable"_i18n;

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("nx_pctl/common/not_set"_i18n);
    else pin->setDetailText(brls::getStr("nx_pctl/dashboard/pin_set", (int)s.pin_length));

    restrictions->setDetailText(ui::bool_text(s.restriction_enabled_ok, s.restriction_enabled,
                                              "nx_pctl/common/yes"_i18n, "nx_pctl/common/no"_i18n));
    temp->setDetailText(ui::bool_text(s.temp_unlocked_ok, s.temp_unlocked,
                                      "nx_pctl/common/yes"_i18n, "nx_pctl/common/no"_i18n));
    temp->setDetailTextColor(s.temp_unlocked_ok && s.temp_unlocked ? ui::color_warn() : ui::color_neutral());

    const bool writable = !app::read_only_build();
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    ui::set_visible(actions_header.getView(), writable);
    ui::set_visible(set_pin.getView(), writable);
    ui::set_visible(unlock.getView(), writable && has_pin && !unlocked);
    ui::set_visible(relock.getView(), writable && unlocked);
    ui::set_visible(danger_header.getView(), writable);
    ui::set_visible(del.getView(), writable);
}

brls::View* SecurityTab::create()
{
    return new SecurityTab();
}
