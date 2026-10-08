// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "view/pt_state_header.hpp"

#include <vector>

#include "ui/ui.hpp"
#include "util/profiles.hpp"

using namespace brls::literals;

PtStateHeader::PtStateHeader()
{
    this->inflateFromXMLRes("xml/view/pt_state_header.xml");
    summary->setSingleLine(false);
    alert->setSingleLine(false);
}

std::string PtStateHeader::configured_text(const PtState& pt)
{
    if (!pt.fw_supported) return "playguard/play_timer/fw_too_old_short"_i18n;
    if (!pt.valid) return "playguard/common/unavailable"_i18n;
    bool any = false, uniform = true;
    for (int i = 0; i < 7; i++) {
        if (pt.day_min[i] != PT_DAY_NOLIMIT) any = true;
        if (pt.day_min[i] != pt.day_min[0]) uniform = false;
    }
    if (!any) return "playguard/play_timer/state/not_set"_i18n;
    const std::string text = uniform
        ? brls::getStr("playguard/play_timer/state/every_day", ui::fmt_minutes(pt.day_min[0]))
        : brls::getStr("playguard/play_timer/state/today_per_day", ui::fmt_minutes(pt.day_min[ui::today_weekday()]));
    // Name the saved profile these limits come from, when one matches exactly.
    const std::string profile = profiles::match(pt.day_min);
    return profile.empty() ? text : brls::getStr("playguard/play_timer/state/profile", profile, text);
}

void PtStateHeader::show(const PtState& pt)
{
    std::vector<std::string> parts;
    const bool reached = pt.fw_supported && pt.restricted_valid && pt.restricted;
    if (pt.fw_supported && pt.enabled_valid) {
        parts.push_back(pt.enabled ? "playguard/play_timer/state/active"_i18n : "playguard/play_timer/state/inactive"_i18n);
        if (pt.enabled && !reached && pt.remaining_valid && pt.remaining_ns > 0)
            parts.push_back(brls::getStr("playguard/play_timer/state/remaining_short", ui::fmt_duration_ns(pt.remaining_ns)));
    }
    parts.push_back(configured_text(pt));
    if (pt.fw_supported && pt.temporary_unlocked_valid && pt.temporary_unlocked)
        parts.push_back("playguard/play_timer/state/unlocked_short"_i18n);

    std::string text;
    for (const auto& p : parts) text += (text.empty() ? "" : " · ") + p;
    summary->setText(text);

    alert->setText(reached ? "playguard/play_timer/state/reached_alert"_i18n : "");
    ui::set_visible(alert, reached);
}

brls::View* PtStateHeader::create()
{
    return new PtStateHeader();
}
