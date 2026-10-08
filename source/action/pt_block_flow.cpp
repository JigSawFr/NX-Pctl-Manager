// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pt_block_flow.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <fmt/format.h>

#include "ui/ui.hpp"
#include "util/diagnostics.hpp"
#include "util/pctl_ops_c.hpp"
#include "util/pt_block.hpp"

using namespace brls::literals;

namespace pt_block_flow
{

namespace
{
std::string firmware()
{
    SysInfo si;
    sysinfo_get(&si);
    return ui::fw_text(si);
}

void save_reference(const pt_block::Block& now)
{
    pt_block::Reference ref;
    ref.block    = now;
    ref.saved_at = ui::now_stamp();
    ref.firmware = firmware();
    std::string err;
    if (pt_block::save_reference(ref, &err)) ui::notify("playguard/dev/pt_block_saved"_i18n);
    else ui::error("playguard/dev/pt_block_save_err"_i18n + ": " + err);
}

void save_report(const pt_block::Reference& ref, const pt_block::Block& now)
{
    std::string err;
    const std::string path = diagnostic::save(pt_block::report(ref, now, ui::now_stamp()), &err);
    if (path.empty()) ui::error("playguard/toast/diag_err"_i18n + ": " + err);
    else ui::notify(brls::getStr("playguard/toast/diag_saved", path));
}
}   // namespace

void open()
{
    PtState pt;
    pctl_play_timer_query(&pt);
    if (!pt.fw_supported) {
        ui::notify(ui::rc_text(NXM_RC_FW_UNSUPPORTED));
        return;
    }
    if (!pt.valid) {
        ui::error("playguard/dev/pt_block_err"_i18n + " — " + ui::rc_text(pt.config_rc));
        return;
    }
    pt_block::Block now;
    static_assert(sizeof(pt.block) == sizeof(now), "PtState.block is the 34 u16 of pt_block::Block");
    std::copy(std::begin(pt.block), std::end(pt.block), now.begin());

    pt_block::Reference ref;
    if (!pt_block::load_reference(&ref)) {
        ui::confirm("playguard/dev/pt_block_intro"_i18n, "playguard/dev/pt_block_save"_i18n,
                    [now]() { save_reference(now); });
        return;
    }

    std::array<std::string, 7> days;
    for (int d = 0; d < 7; d++) days[d] = brls::getStr(fmt::format("playguard/days_short/{}", d));
    const auto changes = pt_block::diff(ref.block, now);
    std::string text;
    if (changes.empty()) {
        text = brls::getStr("playguard/dev/pt_block_same", ref.saved_at);
    } else {
        text = changes.size() == 1 ? brls::getStr("playguard/dev/pt_block_changed_one", ref.saved_at)
                                   : brls::getStr("playguard/dev/pt_block_changed", (int)changes.size(), ref.saved_at);
        text += "\n";
        for (const auto& c : changes)
            text += fmt::format("\n{}: {:04X} → {:04X} ({} → {})", pt_block::field_name(c.index, days),
                                (unsigned)c.before, (unsigned)c.after, (unsigned)c.before, (unsigned)c.after);
    }
    auto* dialog = ui::dialog(text);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->addButton("playguard/dev/pt_block_report"_i18n, [ref, now]() { save_report(ref, now); });
    dialog->addButton("playguard/dev/pt_block_new"_i18n, [now]() { save_reference(now); });
    dialog->open();
}

}   // namespace pt_block_flow
