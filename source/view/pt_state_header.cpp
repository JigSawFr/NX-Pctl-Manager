// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "view/pt_state_header.hpp"

#include "ui/ui.hpp"
#include "util/profiles.hpp"

using namespace brls::literals;

PtStateHeader::PtStateHeader()
{
    this->inflateFromXMLRes("xml/view/pt_state_header.xml");
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
    const std::string na = "playguard/common/unavailable"_i18n;
    if (!pt.fw_supported) {
        for (auto* l : { enabled_value.getView(), restricted_value.getView(), temporary_value.getView(), remaining_value.getView() })
            l->setText("—");
        configured_value->setText(configured_text(pt));
        return;
    }
    enabled_value->setText(ui::bool_text(pt.enabled_valid, pt.enabled, "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    enabled_value->setTextColor(pt.enabled_valid && pt.enabled ? ui::color_ok() : ui::color_text());

    restricted_value->setText(ui::bool_text(pt.restricted_valid, pt.restricted,
        "playguard/play_timer/state/restricted_yes"_i18n, "playguard/common/no"_i18n));
    restricted_value->setTextColor(pt.restricted_valid && pt.restricted ? ui::color_bad() : ui::color_text());

    temporary_value->setText(ui::bool_text(pt.temporary_unlocked_valid, pt.temporary_unlocked,
        "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    temporary_value->setTextColor(pt.temporary_unlocked_valid && pt.temporary_unlocked ? ui::color_warn() : ui::color_text());

    if (!pt.enabled_valid || !pt.remaining_valid) remaining_value->setText(na);
    else if (!pt.enabled) remaining_value->setText("—");
    else remaining_value->setText(ui::fmt_duration_ns(pt.remaining_ns));

    configured_value->setText(configured_text(pt));
}

brls::View* PtStateHeader::create()
{
    return new PtStateHeader();
}
