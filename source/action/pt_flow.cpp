// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/pt_flow.hpp"

#include <borealis.hpp>

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace pt_flow
{

void ready_to_write(std::function<void(bool, bool)> on_ready)
{
    if (app::read_only_build()) {
        ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
        on_ready(false, false);
        return;
    }

    PtState pt;
    pctl_play_timer_query(&pt);
    if (!pt.fw_supported) {
        ui::notify(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
        on_ready(false, false);
        return;
    }
    if (!pt.valid || !pt.enabled_valid || !pt.restricted_valid || !pt.temporary_unlocked_valid) {
        ui::notify(ui::rc_text(NXM_RC_STATE_UNKNOWN));
        on_ready(false, false);
        return;
    }
    if ((!pt.enabled && !pt.restricted) || pt.temporary_unlocked) {
        on_ready(true, false);
        return;
    }

    ui::confirm("nx_pctl/play_timer/gate/body"_i18n, "nx_pctl/play_timer/gate/confirm"_i18n,
        [on_ready]() {
            Result rc = pctl_unlock_restriction_temporarily();
            if (R_FAILED(rc)) {
                ui::notify("nx_pctl/play_timer/gate/failed"_i18n + " — " + ui::rc_text(rc));
                on_ready(false, false);
                return;
            }
            on_ready(true, true);
        },
        [on_ready]() { on_ready(false, false); });
}

void offer_relock(std::function<void()> after)
{
    auto* dialog = new brls::Dialog("nx_pctl/play_timer/relock/body"_i18n);
    dialog->addButton("nx_pctl/play_timer/relock/later"_i18n, [after]() { if (after) after(); });
    dialog->addButton("nx_pctl/play_timer/relock/now"_i18n, [after]() {
        Result rc = pctl_relock();
        ui::notify_result(rc, "nx_pctl/toast/relocked"_i18n, "nx_pctl/toast/relock_err"_i18n);
        if (after) after();
    });
    dialog->open();
}

}   // namespace pt_flow
