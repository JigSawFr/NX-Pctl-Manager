// PlayGuard — borealis entry point: loads the preferences, registers the
// custom XML views, probes the pctl service and shows the tabbed main screen
// (or the init-error screen).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#include <borealis.hpp>
#include <cstdlib>

#include "action/fw_gate.hpp"
#include "action/pin_lock.hpp"
#include "action/pt_log_flow.hpp"
#include "action/rescue.hpp"
#include "activity/init_error_activity.hpp"
#include "activity/lock_activity.hpp"
#include "activity/main_activity.hpp"
#include "activity/rescue_activity.hpp"
#include "app.hpp"
#include "tab/about_tab.hpp"
#include "tab/activity_tab.hpp"
#include "tab/clock_tab.hpp"
#include "tab/dashboard_tab.hpp"
#include "tab/play_timer_tab.hpp"
#include "tab/preferences_tab.hpp"
#include "tab/restrictions_tab.hpp"
#include "tab/security_tab.hpp"
#include "tab/tools_tab.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/own_time.hpp"
#include "view/made_in_france.hpp"
#include "view/scroll_view.hpp"
#include "view/play_days.hpp"
#include "view/pt_gauge.hpp"
#include "view/pt_state_header.hpp"
#include "view/pt_week.hpp"

using namespace brls::literals;

int main(int argc, char* argv[])
{
    app::set_self_path(argc > 0 ? argv[0] : nullptr);

    // Preferences first: the locale must be chosen before borealis loads i18n.
    config::load();
    const auto& cfg = config::get();
    // config::sanitize() keeps the language to config::LANGUAGES.
    if (cfg.language != "system")
        brls::Platform::APP_LOCALE_DEFAULT = cfg.language;
    else
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init borealis Application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("playguard/title"_i18n);
    ui::use_latin_font();

    // Follows the console theme unless a preference says otherwise.
    if (cfg.theme == "dark")
        brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    else if (cfg.theme == "light")
        brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);

    // No cross-fade when a screen opens or closes: on the console's LCD the
    // two screens blended for 200 ms read as ghosting. 2 ms, not 0: borealis
    // still runs (and ends) its animations, in whole milliseconds, and a
    // picker's takes half of it.
    brls::getStyle().addMetric("brls/animations/show", 2.0f);

    // PlayGuard status colours (light / dark variants), before any view exists.
    ui::register_theme_colors();

    // We own the quit path (B on the sidebar / error screen).
    brls::Application::setGlobalQuit(false);

    brls::Application::registerXMLView("ScrollView",      ScrollView::create);
    brls::Application::registerXMLView("PtStateHeader",   PtStateHeader::create);
    brls::Application::registerXMLView("PtGauge",         PtGauge::create);
    brls::Application::registerXMLView("PtWeekView",      PtWeekView::create);
    brls::Application::registerXMLView("PlayDaysView",    PlayDaysView::create);
    brls::Application::registerXMLView("MadeInFrance",    MadeInFrance::create);
    brls::Application::registerXMLView("DashboardTab",    DashboardTab::create);
    brls::Application::registerXMLView("PlayTimerTab",    PlayTimerTab::create);
    brls::Application::registerXMLView("ActivityTab",     ActivityTab::create);
    brls::Application::registerXMLView("RestrictionsTab", RestrictionsTab::create);
    brls::Application::registerXMLView("ClockTab",        ClockTab::create);
    brls::Application::registerXMLView("SecurityTab",     SecurityTab::create);
    brls::Application::registerXMLView("PreferencesTab",  PreferencesTab::create);
    brls::Application::registerXMLView("ToolsTab",        ToolsTab::create);
    brls::Application::registerXMLView("AboutTab",        AboutTab::create);

    if (app::init()) {
        // Untested firmware: read-only (or the remembered choice) before any tab is built.
        fw_gate::prepare();
        // Security › Ask for the PIN: checked before every change from now on;
        // "To open PlayGuard" starts on the lock screen.
        pin_lock::install();
        // The playguard-rescue sysmodule acted on a RESCUE file: show what it
        // did and let the parent finish, before (and instead of) the lock
        // screen — they are here because they forgot the PIN.
        if (auto report = rescue::take())
            brls::Application::pushActivity(new RescueActivity(*report));
        else if (pin_lock::at_start())
            brls::Application::pushActivity(new LockActivity());
        else
            brls::Application::pushActivity(new MainActivity());
    } else {
        brls::Logger::error("pctl probe failed (0x{:08X}) — showing InitErrorActivity", app::pctl_init_result());
        brls::Application::pushActivity(new InitErrorActivity());
    }

    // Started over a game, PlayGuard's time counts as that game's: noted, so
    // the Activity tab can leave it out.
    {
        SysInfo si;
        sysinfo_get(&si);
        own_time::start(!si.applet_mode);
    }
    pt_log_flow::apply();   // Developer › Record the play timer, when on

    while (brls::Application::mainLoop())
        ;

    pt_log_flow::stop();
    own_time::stop();
    app::shutdown();
    http::cleanup();
    return EXIT_SUCCESS;
}
