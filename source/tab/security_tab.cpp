// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/security_tab.hpp"

#include "action/backup_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
void wipe(char* p, size_t n)
{
    volatile char* b = p;
    while (n--) *b++ = 0;
}

// The PIN in large digits. Our copies are wiped as soon as the label holds
// the text, and the label goes away with the dialog. Only the result code is
// ever logged.
void show_pin_dialog()
{
    char pin[16];
    Result rc = pctl_get_pin(pin, sizeof(pin));
    brls::Logger::info("pctl_get_pin returned 0x{:08X}", (unsigned)rc);
    if (R_FAILED(rc)) {
        ui::notify_result(rc, "", "playguard/security/show_pin_err"_i18n);
        return;
    }
    std::string spaced;   // "1 2 3 4": easier to read out and to type
    spaced.reserve(2 * sizeof(pin));   // no reallocation, so no stray copy
    for (const char* c = pin; *c; c++) {
        if (!spaced.empty()) spaced += ' ';
        spaced += *c;
    }
    wipe(pin, sizeof(pin));

    auto* title = new brls::Label();
    title->setText("playguard/security/show_pin_title"_i18n);
    title->setFontSize(22);
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    title->setTextColor(ui::color_note());
    auto* digits = new brls::Label();
    digits->setText(spaced);
    digits->setFontSize(56);
    digits->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    digits->setMarginTop(16);
    wipe(&spaced[0], spaced.size());

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(40, 40, 40, 40);
    box->addView(title);
    box->addView(digits);

    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->setCancelable(true);
    dialog->open();
}
}   // namespace

SecurityTab::SecurityTab()
    : TabBase("xml/tab/security.xml")
{
    this->enable_auto_refresh(5000);
    pr_note->setSingleLine(false);
    pr_unlink->registerClickAction([this](brls::View*) {
        ui::confirm_danger("playguard/pairing/unlink_body"_i18n, "playguard/pairing/unlink_confirm"_i18n, [this]() {
            Result rc = pctl_delete_pairing();
            ui::notify_result(rc, "playguard/pairing/unlinked"_i18n, "playguard/pairing/unlink_err"_i18n);
            this->refresh();
        });
        return true;
    });
    set_pin->registerClickAction([this](brls::View*) {
        // Blocks while the system PIN applet is shown; the session is released first.
        Result rc = pctl_set_pin();
        brls::Logger::info("pctl_set_pin returned 0x{:08X}", (unsigned)rc);
        ui::notify_result(rc, "playguard/security/pin_ok"_i18n, "playguard/security/pin_err"_i18n);
        this->refresh();
        return true;
    });
    show_pin->registerClickAction([](brls::View*) {
        ui::confirm("playguard/security/show_pin_body"_i18n, "playguard/security/show_pin_confirm"_i18n,
                    []() { brls::sync([]() { show_pin_dialog(); }); });
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
                    // A backup first, so the settings can come back (Tools › Restore).
                    backup_flow::backup_then([this]() {
                        Result rc = pctl_delete_parental_controls();
                        ui::notify_result(rc, "playguard/security/deleted"_i18n, "playguard/security/delete_err"_i18n);
                        this->refresh();
                    });
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
    ui::note_unlocked(s.temp_unlocked_ok, s.temp_unlocked);
    const std::string na = "playguard/common/unavailable"_i18n;

    if (!s.pin_length_ok) pin->setDetailText(na);
    else if (s.pin_length == 0) pin->setDetailText("playguard/common/not_set"_i18n);
    else pin->setDetailText(brls::getStr("playguard/dashboard/pin_set", (int)s.pin_length));

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

    const bool writable = !app::read_only();
    const bool has_pin  = s.pin_length_ok && s.pin_length > 0;
    const bool unlocked = s.temp_unlocked_ok && s.temp_unlocked;
    ui::set_visible_all({ { actions_header.getView(), writable },
                          { set_pin.getView(), writable },
                          { show_pin.getView(), writable && has_pin },
                          { unlock.getView(), writable && has_pin && !unlocked },
                          { relock.getView(), writable && unlocked },
                          // Nothing to unlink when the read says not linked.
                          { pr_unlink.getView(), writable && (!s.pairing_active_ok || paired) },
                          { danger_header.getView(), writable },
                          { del.getView(), writable } });
}

brls::View* SecurityTab::create()
{
    return new SecurityTab();
}
