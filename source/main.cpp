// PlayGuard — borealis entry point: loads the preferences, registers the
// custom XML views, probes the pctl service and shows the tabbed main screen
// (or the init-error screen).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#include <borealis.hpp>
#include <cstdlib>

#include "activity/init_error_activity.hpp"
#include "activity/main_activity.hpp"
#include "app.hpp"
#include "tab/activity_tab.hpp"
#include "tab/clock_tab.hpp"
#include "tab/dashboard_tab.hpp"
#include "tab/play_timer_tab.hpp"
#include "tab/restrictions_tab.hpp"
#include "tab/security_tab.hpp"
#include "tab/tools_tab.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/pt_gauge.hpp"
#include "view/pt_state_header.hpp"
#include "view/pt_week.hpp"

using namespace brls::literals;

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Preferences first: the locale must be chosen before borealis loads i18n.
    config::load();
    const auto& cfg = config::get();
    if (cfg.language == "en-US" || cfg.language == "fr")
        brls::Platform::APP_LOCALE_DEFAULT = cfg.language;
    else
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init borealis Application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow(app::read_only_build() ? "playguard/title_read_only"_i18n : "playguard/title"_i18n);

    // Follows the console theme unless a preference says otherwise.
    if (cfg.theme == "dark")
        brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    else if (cfg.theme == "light")
        brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);

    // PlayGuard status colours (light / dark variants), before any view exists.
    ui::register_theme_colors();

    // We own the quit path (B on the sidebar / error screen).
    brls::Application::setGlobalQuit(false);

    brls::Application::registerXMLView("PtStateHeader",   PtStateHeader::create);
    brls::Application::registerXMLView("PtGauge",         PtGauge::create);
    brls::Application::registerXMLView("PtWeekView",      PtWeekView::create);
    brls::Application::registerXMLView("DashboardTab",    DashboardTab::create);
    brls::Application::registerXMLView("PlayTimerTab",    PlayTimerTab::create);
    brls::Application::registerXMLView("ActivityTab",     ActivityTab::create);
    brls::Application::registerXMLView("RestrictionsTab", RestrictionsTab::create);
    brls::Application::registerXMLView("ClockTab",        ClockTab::create);
    brls::Application::registerXMLView("SecurityTab",     SecurityTab::create);
    brls::Application::registerXMLView("ToolsTab",        ToolsTab::create);

    if (app::init()) {
        brls::Application::pushActivity(new MainActivity());
    } else {
        brls::Logger::error("pctl probe failed (0x{:08X}) — showing InitErrorActivity", app::pctl_init_result());
        brls::Application::pushActivity(new InitErrorActivity());
    }

    while (brls::Application::mainLoop())
        ;

    app::shutdown();
    return EXIT_SUCCESS;
}
