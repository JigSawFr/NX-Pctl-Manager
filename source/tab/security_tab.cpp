// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/security_tab.hpp"

#include "action/backup_flow.hpp"
#include "action/console_lock.hpp"
#include "action/history_flow.hpp"
#include "action/pin_lock.hpp"
#include "action/rescue.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

SecurityTab::SecurityTab()
    : TabBase("xml/tab/security.xml")
{
    this->enable_auto_refresh(5000);
    pr_note->setSingleLine(false);
    rescue_note->setSingleLine(false);
    // Checked once: the sysmodule only comes and goes with the SD card out.
    ui::set_visible(rescue_note.getView(), rescue::installed());
    pin_lock_note->setSingleLine(false);
    console_lock_note->setSingleLine(false);
    console_lock_cell->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        PtState pt;
        pctl_play_timer_query(&pt);
        if (!pt.fw_supported) {
            ui::error(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
            return true;
        }
        PctlStatus s;
        pctl_status_fetch(&s);
        if (!(s.pin_length_ok && s.pin_length > 0)) {   // nothing gates play without a PIN
            ui::info("playguard/console_lock/needs_pin"_i18n);
            return true;
        }
        console_lock::set(!console_lock::active(), pt, [this]() { this->refresh(); });
        return true;
    });
    // Deleting everything is the one irreversible action here: its line is
    // drawn in the danger colour, not only its section title (refresh()).
    pin_lock_cell->registerClickAction([this](brls::View*) {
        pin_lock::choose([this]() { this->refresh(); });
        return true;
    });
    pr_unlink->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        // Developer tools: the block comparison needs the link, so say it goes.
        std::string body = "playguard/pairing/unlink_body"_i18n;
        if (config::get().dev_mode) body += "\n\n" + "playguard/pairing/unlink_dev_note"_i18n;
        ui::confirm_danger(body, "playguard/pairing/unlink_confirm"_i18n, [this]() {
            Result rc = pctl_delete_pairing();
            if (R_SUCCEEDED(rc)) history_flow::record_event("unlink");
            ui::notify_result(rc, "playguard/pairing/unlinked"_i18n, "playguard/pairing/unlink_err"_i18n);
            this->refresh();
        });
        return true;
    });
    set_pin->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        this->change_pin();
        return true;
    });
    show_pin->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        ui::confirm("playguard/security/show_pin_body"_i18n, "playguard/security/show_pin_confirm"_i18n, [this]() {
            brls::sync([this]() {
                // The PIN every time (pin_lock.hpp); then, as it may have been
                // seen over a shoulder, a new one is offered.
                if (!pin_lock::before_show_pin()) return;
                const bool shown = ui::show_pin_dialog([this]() {
                    brls::sync([this]() {
                        ui::confirm("playguard/security/show_pin_change_body"_i18n,
                                    "playguard/security/show_pin_change_confirm"_i18n, [this]() {
                                        brls::sync([this]() { this->change_pin(); });
                                    });
                    });
                });
                if (shown) history_flow::record_event("pin_shown");
            });
        });
        return true;
    });
    unlock->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        ui::confirm("playguard/security/unlock_body"_i18n, "playguard/security/unlock_confirm"_i18n, [this]() {
            Result rc = pctl_unlock_restriction_temporarily();
            if (R_SUCCEEDED(rc)) history_flow::record_event("unlock");
            ui::notify_result(rc, "playguard/security/unlocked"_i18n, "playguard/security/unlock_err"_i18n);
            this->refresh();
        });
        return true;
    });
    relock->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        Result rc = pctl_relock();
        if (R_SUCCEEDED(rc)) history_flow::record_event("relock");
        ui::notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        this->refresh();
        return true;
    });
    del->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        ui::confirm_danger("playguard/security/delete_body"_i18n, "playguard/security/delete_confirm"_i18n, [this]() {
            // Second, separate confirmation with a different button: this cannot be undone.
            brls::sync([this]() {
                ui::confirm_danger("playguard/security/delete_body2"_i18n, "playguard/security/delete_confirm2"_i18n, [this]() {
                    // A backup first, so the settings can come back (Tools › Restore).
                    backup_flow::backup_then([this]() {
                        Result rc = pctl_delete_parental_controls();
                        if (R_SUCCEEDED(rc)) history_flow::record_event("delete");
                        ui::notify_result(rc, "playguard/security/deleted"_i18n, "playguard/security/delete_err"_i18n);
                        this->refresh();
                    });
                });
            });
        });
        return true;
    });
}

void SecurityTab::change_pin()
{
    // Blocks while the system PIN applet is shown; the session is released first.
    Result rc = pctl_set_pin();
    brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
    if (R_SUCCEEDED(rc)) history_flow::record_event("pin");
    ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
    this->refresh();
}

void SecurityTab::refresh()
{
    PctlStatus s;
    pctl_status_fetch(&s);
    ui::note_unlocked(s.temp_unlocked_ok, s.temp_unlocked);
    const std::string na = "playguard/common/unavailable"_i18n;

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("playguard/common/not_set"_i18n);
    else pin->setDetailText("playguard/dashboard/pin_set"_i18n);   // not its length: it narrows a guess

    restrictions->setDetailText(ui::bool_text(s.restriction_enabled_ok, s.restriction_enabled,
                                              "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    temp->setDetailText(ui::bool_text(s.temp_unlocked_ok, s.temp_unlocked,
                                      "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    temp->setDetailTextColor(s.temp_unlocked_ok && s.temp_unlocked ? ui::color_warn() : ui::color_neutral());

    // Companion app: while linked, its next sync overwrites what is set here.
    const bool paired = s.pairing_active_ok && s.pairing_active;
    pr_active->setDetailText(ui::bool_text(s.pairing_active_ok, s.pairing_active,
                                           "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    pr_active->setDetailTextColor(paired ? ui::color_warn() : ui::color_neutral());
    pr_updated->setDetailText(s.last_updated_ok ? ui::time_text(s.last_updated) : "playguard/common/unavailable"_i18n);
    pr_note->setTextColor(paired ? ui::color_warn() : ui::color_note());

    pin_lock_cell->setDetailText(pin_lock::mode_text());
    pin_lock_note->setText(pin_lock::note_text());

    const bool locked = console_lock::active();
    console_lock_cell->setDetailText(locked ? "playguard/common/on"_i18n : "playguard/common/off"_i18n);
    console_lock_cell->setDetailTextColor(locked ? ui::color_warn() : ui::color_neutral());
    console_lock_note->setText(locked ? "playguard/console_lock/note_on"_i18n : "playguard/console_lock/note_off"_i18n);

    const bool writable = !app::read_only();
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    // Read-only: the actions stay in sight, greyed (A says why).
    ui::set_visible_all({ { show_pin.getView(), has_pin },
                          { unlock.getView(), has_pin && !unlocked },
                          { relock.getView(), unlocked },
                          // Nothing to unlink when the read says not linked.
                          { pr_unlink.getView(), !s.pairing_active_ok || paired } });
    for (brls::DetailCell* c : { (brls::DetailCell*)set_pin.getView(), (brls::DetailCell*)show_pin.getView(),
                                 (brls::DetailCell*)unlock.getView(), (brls::DetailCell*)relock.getView(),
                                 (brls::DetailCell*)console_lock_cell.getView(), (brls::DetailCell*)pr_unlink.getView() })
        ui::show_writable(c, writable);
    ui::show_writable(del, writable, ui::color_bad(), ui::color_neutral());
}

brls::View* SecurityTab::create()
{
    return new SecurityTab();
}
