// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/pairing_tab.hpp"

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

PairingTab::PairingTab()
    : TabBase("xml/tab/pairing.xml")
{
    note->setSingleLine(false);
    unlink->registerClickAction([this](brls::View*) {
        ui::confirm("playguard/pairing/unlink_body"_i18n, "playguard/pairing/unlink_confirm"_i18n, [this]() {
            Result rc = pctl_delete_pairing();
            ui::notify_result(rc, "playguard/pairing/unlinked"_i18n, "playguard/pairing/unlink_err"_i18n);
            this->refresh();
        });
        return true;
    });
}

void PairingTab::refresh()
{
    PctlStatus s;
    pctl_status_fetch(&s);
    active->setDetailText(ui::bool_text(s.pairing_active_ok, s.pairing_active,
                                        "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    active->setDetailTextColor(s.pairing_active_ok && s.pairing_active ? ui::color_warn() : ui::color_neutral());
    updated->setDetailText(s.last_updated_ok ? ui::time_text(s.last_updated) : "playguard/common/unavailable"_i18n);
    ui::set_visible(unlink.getView(), !app::read_only_build());
}

brls::View* PairingTab::create()
{
    return new PairingTab();
}
