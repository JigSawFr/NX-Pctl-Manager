// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/rescue_activity.hpp"

#include "action/backup_flow.hpp"
#include "action/history_flow.hpp"
#include "action/pin_lock.hpp"
#include "activity/lock_activity.hpp"
#include "activity/main_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

RescueActivity::RescueActivity(RescueReport report)
    : report(report)
{
}

void RescueActivity::onContentAvailable()
{
    outcome->setSingleLine(false);
    note->setSingleLine(false);

    // The recovery screen is the trusted context (see the header): no PIN is
    // asked before the changes made here.
    core_set_change_check(nullptr);

    show_pin->registerClickAction([](brls::View*) {
        if (ui::refuse_read_only()) return true;
        brls::sync([]() { ui::show_pin_dialog(); });
        return true;
    });
    reset_pin->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        Result rc = pctl_set_pin();   // the system PIN screen, which sets a new one
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        if (R_SUCCEEDED(rc)) history_flow::record_event("pin", "rescue");
        ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    del->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        ui::confirm_danger("playguard/security/delete_body"_i18n, "playguard/security/delete_confirm"_i18n, [this]() {
            brls::sync([this]() {
                ui::confirm_danger("playguard/security/delete_body2"_i18n, "playguard/security/delete_confirm2"_i18n, [this]() {
                    backup_flow::backup_then([this]() {
                        Result rc = pctl_delete_parental_controls();
                        if (R_SUCCEEDED(rc)) history_flow::record_event("delete", "rescue");
                        ui::notify_result(rc, "playguard/security/deleted"_i18n, "playguard/security/delete_err"_i18n);
                        this->refresh();
                    });
                });
            });
        });
        return true;
    });
    cont->registerClickAction([this](brls::View*) {
        this->proceed();
        return true;
    });
    // B continues too (rather than quitting): the app should open after a rescue.
    this->getContentView()->registerAction("playguard/rescue/continue"_i18n, brls::BUTTON_B, [this](brls::View*) {
        this->proceed();
        return true;
    });

    this->refresh();
}

void RescueActivity::willAppear(bool resetState)
{
    brls::Activity::willAppear(resetState);
    this->refresh();
}

void RescueActivity::refresh()
{
    const bool ok      = report.result == RescueResult_Ok;
    const bool deleted = ok && report.mode == RescueMode_Delete;

    // What the sysmodule did, in the status colour that matches it.
    const char* key = deleted                               ? "playguard/rescue/outcome/deleted"
                    : ok                                    ? "playguard/rescue/outcome/unlocked"
                    : report.result == RescueResult_NoPin   ? "playguard/rescue/outcome/no_pin"
                                                            : "playguard/rescue/outcome/failed";
    outcome->setText(brls::getStr(key));
    outcome->setTextColor(ok ? ui::color_ok()
                             : report.result == RescueResult_NoPin ? ui::color_warn() : ui::color_bad());

    std::string n = deleted                               ? "playguard/rescue/note_deleted"_i18n
                  : ok                                    ? "playguard/rescue/note"_i18n
                  : report.result == RescueResult_NoPin   ? "playguard/rescue/note_no_pin"_i18n
                                                          : "playguard/rescue/note_failed"_i18n;
    if (report.result == RescueResult_Failed)
        n += "\n" + brls::getStr("playguard/rescue/failed_code", ui::rc_text(report.rc));
    if (app::read_only()) n += "\n" + "playguard/common/read_only_note"_i18n;
    note->setText(n);

    // After a delete there is nothing left to show, reset or delete: only
    // "Open PlayGuard" remains, so the actions section is hidden entirely.
    PctlStatus s;
    pctl_status_fetch(&s);
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool writable = !app::read_only();

    // Short hints on the right, so a parent knows what each one does at a glance.
    show_pin->setDetailText("playguard/rescue/hint/show_pin"_i18n);
    reset_pin->setDetailText("playguard/rescue/hint/reset_pin"_i18n);
    del->setDetailText("playguard/rescue/hint/delete"_i18n);
    for (brls::DetailCell* c : { (brls::DetailCell*)show_pin.getView(),
                                 (brls::DetailCell*)reset_pin.getView(), (brls::DetailCell*)del.getView() })
        c->setDetailTextColor(ui::color_note());

    // After the sysmodule deleted everything, only "Open PlayGuard" is left.
    // Otherwise: show the PIN and delete need one to exist; "Set a new PIN"
    // is always offered (it sets one when there is none).
    const bool actions = !deleted;
    ui::set_visible_all({ { actions_header.getView(), actions },
                          { show_pin.getView(), actions && has_pin },
                          { reset_pin.getView(), actions },
                          { del.getView(), actions && has_pin } });
    for (brls::DetailCell* c : { (brls::DetailCell*)show_pin.getView(),
                                 (brls::DetailCell*)reset_pin.getView() })
        ui::show_writable(c, writable);
    ui::show_writable(del, writable, ui::color_bad(), ui::color_neutral());
}

void RescueActivity::proceed()
{
    // Restore the normal PIN-before-a-change gate, then open the app. If the
    // parent reset the PIN, "ask to open" would apply next launch; not now.
    pin_lock::install();
    brls::Application::popActivity(brls::TransitionAnimation::NONE, []() {
        brls::Application::pushActivity(new MainActivity());
    });
}
