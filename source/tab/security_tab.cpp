// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/security_tab.hpp"

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

SecurityTab::SecurityTab()
    : TabBase("xml/tab/security.xml")
{
    this->enable_auto_refresh(5000);
    set_pin->registerClickAction([this](brls::View*) {
        // Blocks while the system PIN applet is shown; the session is released first.
        Result rc = pctl_set_pin();
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    unlock->registerClickAction([this](brls::View*) {
        ui::confirm("playguard/security/unlock_body"_i18n, "playguard/security/unlock_confirm"_i18n, [this]() {
            Result rc = pctl_unlock_restriction_temporarily();
            ui::notify_result(rc, "playguard/security/unlocked"_i18n, "playguard/security/unlock_err"_i18n);
            this->refresh();
        });
        return true;
    });
    relock->registerClickAction([this](brls::View*) {
        Result rc = pctl_relock();
        ui::notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        this->refresh();
        return true;
    });
    del->registerClickAction([this](brls::View*) {
        ui::confirm_danger("playguard/security/delete_body"_i18n, "playguard/security/delete_confirm"_i18n, [this]() {
            // Second, separate confirmation with a different button: this cannot be undone.
            brls::sync([this]() {
                ui::confirm_danger("playguard/security/delete_body2"_i18n, "playguard/security/delete_confirm2"_i18n, [this]() {
                    Result rc = pctl_delete_parental_controls();
                    ui::notify_result(rc, "playguard/security/deleted"_i18n, "playguard/security/delete_err"_i18n);
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
    const std::string na = "playguard/common/unavailable"_i18n;

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("playguard/common/not_set"_i18n);
    else pin->setDetailText(brls::getStr("playguard/dashboard/pin_set", (int)s.pin_length));

    restrictions->setDetailText(ui::bool_text(s.restriction_enabled_ok, s.restriction_enabled,
                                              "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    temp->setDetailText(ui::bool_text(s.temp_unlocked_ok, s.temp_unlocked,
                                      "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    temp->setDetailTextColor(s.temp_unlocked_ok && s.temp_unlocked ? ui::color_warn() : ui::color_neutral());

    const bool writable = !app::read_only_build();
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    ui::set_visible_all({ { actions_header.getView(), writable },
                          { set_pin.getView(), writable },
                          { unlock.getView(), writable && has_pin && !unlocked },
                          { relock.getView(), writable && unlocked },
                          { danger_header.getView(), writable },
                          { del.getView(), writable } });
}

brls::View* SecurityTab::create()
{
    return new SecurityTab();
}
