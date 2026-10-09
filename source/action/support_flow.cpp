// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/support_flow.hpp"

#include "activity/whats_new_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/support.hpp"
#include "view/funding.hpp"
#include "view/release_notes.hpp"

using namespace brls::literals;

namespace support_flow
{
namespace
{
// Long enough for the start-up screens and questions to be on top already.
constexpr long START_DELAY_MS = 1500;

void show_reminder()
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->setPadding(28, 32, 12, 32);
    auto* title = new brls::Label();
    title->setFontSize(26);
    title->setText("playguard/about/reminder_title"_i18n);
    box->addView(title);
    auto* body = new brls::Label();
    body->setSingleLine(false);
    body->setFontSize(20);
    body->setMarginTop(10);
    body->setText("playguard/about/support_body"_i18n);
    box->addView(body);
    brls::Box* codes = funding::cards(160, false);
    codes->setMarginTop(12);
    box->addView(codes);

    auto* dialog = new brls::Dialog(box);
    dialog->addButton("playguard/common/later"_i18n, []() {});
    dialog->addButton("playguard/about/reminder_never"_i18n, []() {
        config::get().support_reminder = false;
        if (ui::save_config()) ui::notify("playguard/about/reminder_off"_i18n);
    });
    dialog->open();
}
}   // namespace

void at_start(brls::Activity* main)
{
    brls::delay(START_DELAY_MS, [main]() {
        auto stack = brls::Application::getActivitiesStack();
        if (stack.empty() || stack.back() != main) return;   // something else first: next start

        auto& cfg = config::get();
        const support::State state{ cfg.support_reminder, cfg.support_reminded, cfg.seen_version };
        const support::Decision d =
            support::decide(state, app::version(), ui::today_date(), !release_notes::current().empty());
        if (d.next.reminded != state.reminded || d.next.seen_version != state.seen_version) {
            cfg.support_reminded = d.next.reminded;
            cfg.seen_version     = d.next.seen_version;
            ui::save_config();
        }
        if (d.show == support::Show::WhatsNew) brls::Application::pushActivity(new WhatsNewActivity());
        else if (d.show == support::Show::Reminder) show_reminder();
    });
}

}   // namespace support_flow
