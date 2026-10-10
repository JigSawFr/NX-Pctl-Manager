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

RescueActivity::RescueActivity(RescueReport report, bool confirmed)
    : report(report), confirmed(confirmed)
{
}

void RescueActivity::onContentAvailable()
{
    outcome->setSingleLine(false);
    note->setSingleLine(false);

    // A confirmed recovery is the trusted context (see the header): no PIN is
    // asked before the changes made here. Otherwise the actions are hidden and
    // the usual check stays.
    if (confirmed) core_set_change_check(nullptr);

    reset_pin->registerClickAction([this](brls::View*) {
        if (!confirmed || ui::refuse_read_only()) return true;
        Result rc = pctl_set_pin();   // the system PIN screen, which sets a new one
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        if (R_SUCCEEDED(rc)) history_flow::record_event("pin", "rescue");
        ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    del->registerClickAction([this](brls::View*) {
        if (!confirmed || ui::refuse_read_only()) return true;
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
    // A report the console does not confirm says nothing true about it: say
    // so instead of what the file claims (a failure or a refusal changed
    // nothing, so its own words stand).
    const bool claimed   = report.result == RescueResult_Ok || report.result == RescueResult_NoPin;
    const bool doubted   = claimed && !confirmed;
    const bool ok        = report.result == RescueResult_Ok && confirmed;
    const bool deleted   = ok && report.mode == RescueMode_Delete;

    // What the sysmodule did, in the status colour that matches it.
    const char* key = doubted                               ? "playguard/rescue/outcome/unconfirmed"
                    : deleted                               ? "playguard/rescue/outcome/deleted"
                    : ok                                    ? "playguard/rescue/outcome/unlocked"
                    : report.result == RescueResult_NoPin   ? "playguard/rescue/outcome/no_pin"
                    : report.result == RescueResult_Refused ? "playguard/rescue/outcome/refused"
                                                            : "playguard/rescue/outcome/failed";
    outcome->setText(brls::getStr(key));
    outcome->setTextColor(ok ? ui::color_ok()
                             : report.result == RescueResult_NoPin && !doubted ? ui::color_warn() : ui::color_bad());

    std::string n = doubted                               ? "playguard/rescue/note_unconfirmed"_i18n
                  : deleted                               ? "playguard/rescue/note_deleted"_i18n
                  : ok                                    ? "playguard/rescue/note"_i18n
                  : report.result == RescueResult_NoPin   ? "playguard/rescue/note_no_pin"_i18n
                  : report.result == RescueResult_Refused ? "playguard/rescue/note_refused"_i18n
                                                          : "playguard/rescue/note_failed"_i18n;
    if (report.result == RescueResult_Failed)
        n += "\n" + brls::getStr("playguard/rescue/failed_code", ui::rc_text(report.rc, false));   // the note says what next
    // The request file is still on the card: an unlock request unlocks again
    // at every start until it is removed from a computer.
    if (report.request == RescueRequest_Kept && report.result != RescueResult_Refused)
        n += "\n\n" + brls::getStr("playguard/rescue/request_kept", RESCUE_REQUEST_NAME);
    if (app::read_only()) n += "\n" + "playguard/common/read_only_note"_i18n;
    note->setText(n);

    // After a delete there is nothing left to show, reset or delete: only
    // "Open PlayGuard" remains, so the actions section is hidden entirely.
    PctlStatus s;
    pctl_status_fetch(&s);
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool writable = !app::read_only();

    // Short hints on the right, so a parent knows what each one does at a glance.
    reset_pin->setDetailText("playguard/rescue/hint/reset_pin"_i18n);
    del->setDetailText("playguard/rescue/hint/delete"_i18n);
    for (brls::DetailCell* c : { (brls::DetailCell*)reset_pin.getView(), (brls::DetailCell*)del.getView() })
        c->setDetailTextColor(ui::color_note());

    // After the sysmodule deleted everything, only "Open PlayGuard" is left.
    // Otherwise: delete needs a PIN to exist; "Set a new PIN" is always
    // offered (it sets one when there is none).
    // Nothing at all unless the console confirmed the recovery.
    const bool actions = confirmed && !deleted;
    ui::set_visible_all({ { actions_header.getView(), actions },
                          { reset_pin.getView(), actions },
                          { del.getView(), actions && has_pin } });
    ui::show_writable(reset_pin, writable);
    ui::show_writable(del, writable, ui::color_bad(), ui::color_neutral());
}

void RescueActivity::proceed()
{
    // Restore the normal PIN-before-a-change gate, then open the app. If the
    // parent reset the PIN, "ask to open" would apply next launch; not now.
    // An unconfirmed report skips nothing: the lock screen comes first, as on
    // any start.
    pin_lock::install();
    if (!confirmed && pin_lock::at_start())
        ui::replace_screen(new LockActivity());
    else
        ui::replace_screen(new MainActivity());
}
