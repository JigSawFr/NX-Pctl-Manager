// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/agent_update.hpp"
#include "action/clock_check.hpp"
#include "action/fw_gate.hpp"
#include "action/pt_flow.hpp"
#include "action/support_flow.hpp"
#include "action/update_flow.hpp"
#include "activity/modules_activity.hpp"
#include "activity/onboarding_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

// Nine tabs and three separators do not fit the 720p sidebar at borealis'
// sizes (70 px items, 30 px separators): "Tools" and "About" fell below the
// edge (559 px between the header and the footer: 20 + 9 × 54 + 3 × 16 = 554).
// Tighter items there only; detail cells keep the shared 70 px.
static void compact_sidebar(brls::View* tab_frame)
{
    auto* sidebar = tab_frame ? dynamic_cast<brls::Sidebar*>(tab_frame->getView("brls/tab_frame/sidebar")) : nullptr;
    brls::SidebarItem* first = sidebar ? sidebar->getItem(0) : nullptr;
    brls::Box* list = first ? first->getParent() : nullptr;
    if (!list) return;
    list->setPaddingTop(10);
    list->setPaddingBottom(10);
    for (brls::View* v : list->getChildren()) {
        if (dynamic_cast<brls::SidebarItem*>(v)) v->setHeight(54);
        else v->setHeight(16);   // a separator: its line is drawn at mid-height
    }
}

void MainActivity::onContentAvailable()
{
    this->update_title();
    compact_sidebar(this->getView("main_tabs"));

    // B on the sidebar quits (the tabs themselves send B back to the sidebar),
    // but only when pressed twice within 2 s: one stray B never closes the app.
    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        static brls::Time last_press = 0;
        const brls::Time now = brls::getCPUTimeUsec();
        if (last_press && now - last_press < 2000000) {
            brls::Application::quit();
            return true;
        }
        last_press = now;
        brls::Application::notify("playguard/hints/exit_again"_i18n);
        return true;
    });

    // Tools › Start on: that tab rather than the Overview (first, so the
    // questions below keep the focus they take).
    int start = 0;
    for (const char* t : config::START_TABS) {
        if (config::get().start_tab == t) break;
        start++;
    }
    if (start > 0 && start < (int)(sizeof(config::START_TABS) / sizeof(config::START_TABS[0]))) {   // ui::tab::of knows them all
        brls::sync([this, start]() {
            auto stack = brls::Application::getActivitiesStack();
            auto* frame = dynamic_cast<brls::TabFrame*>(this->getView("main_tabs"));
            if (frame && !stack.empty() && stack.back() == this) frame->focusTab(ui::tab::of(start));
        });
    }

    // Stopped in the middle of a change last time: lock again first. Then, for
    // extra time added on an earlier day, offer to put the limit back.
    brls::sync([]() {
        pt_flow::relock_if_interrupted();
        pt_flow::offer_extra_time_restore();
    });
    // Opt-in background checks (Tools): a newer version, a network clock off.
    update_flow::check_daily();
    clock_check::at_start();

    // Firmware newer than the checked one: the firmware screen (or the remembered choice).
    fw_gate::on_main_screen();
    // Nothing set up yet (no PIN): the first steps, once per start.
    if (OnboardingActivity::wanted_at_start())
        brls::sync([]() { brls::Application::pushActivity(new OnboardingActivity()); });
    // Then, with nothing else in front: "What's new" after an update, else the
    // monthly "Support PlayGuard" reminder.
    support_flow::at_start(this);
    // The agent sysmodule on the SD card is not the one this PlayGuard
    // carries: the offer to update it.
    agent_update::at_start(this, ModulesActivity::bundle_dir());

    this->day = ui::today_date();
    this->day_timer.setPeriod(30000);
    this->day_timer.setCallback([this]() {
        // Only on the main screen, not behind a dialog: the date is compared
        // again at the next tick otherwise.
        auto stack = brls::Application::getActivitiesStack();
        if (!app::in_focus() || stack.empty() || stack.back() != this) return;
        const std::string today = ui::today_date();
        if (today == this->day) return;
        this->day = today;
        pt_flow::offer_extra_time_restore();
    });
    this->day_timer.start();
}

void MainActivity::update_title()
{
    // "PlayGuard", then what the user must know whatever the tab: read-only,
    // temporarily unlocked (the play timer is not counting).
    std::string title = "playguard/title"_i18n;
    if (app::read_only()) title += " · " + "playguard/title_tags/read_only"_i18n;
    if (ui::known_unlocked()) title += " · " + "playguard/title_tags/unlocked"_i18n;
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) frame->setTitle(title);
}
