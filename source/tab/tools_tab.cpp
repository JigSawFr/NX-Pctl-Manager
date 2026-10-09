// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tools_tab.hpp"

#include <fmt/format.h>

#include "action/backup_flow.hpp"
#include "action/fw_gate.hpp"
#include "action/pt_block_flow.hpp"
#include "activity/diagnostic_activity.hpp"
#include "activity/firmware_gate_activity.hpp"
#include "activity/history_activity.hpp"
#include "activity/onboarding_activity.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/history.hpp"
#include "util/patches.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace
{
std::string keep_text(int keep)
{
    return keep == 0 ? "playguard/tools/backup_keep_all"_i18n : brls::getStr("playguard/tools/backup_keep_n", keep);
}
}   // namespace

ToolsTab::ToolsTab()
    : TabBase("xml/tab/tools.xml")
{
    export_note->setSingleLine(false);
    backup_note->setSingleLine(false);
    serial_note->setSingleLine(false);
    patches_note->setSingleLine(false);

    first_steps->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new OnboardingActivity());
        return true;
    });
    history->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new HistoryActivity());
        return true;
    });

    // Ⓐ on the serial number shows / hides the masked digits.
    serial->registerAction("playguard/tools/serial_show"_i18n, brls::BUTTON_A, [this](brls::View*) {
        this->serial_revealed = !this->serial_revealed;
        this->refresh();
        return true;
    }, false, false, brls::SOUND_CLICK);
    export_note->setText(brls::getStr("playguard/tools/export_note", paths::logs_dir()));

    export_cell->registerClickAction([](brls::View*) {
        std::string err;
        std::string path = diagnostic::save(diagnostic::current_report(), &err);
        if (path.empty()) ui::error("playguard/toast/diag_err"_i18n + ": " + err);
        else ui::notify(brls::getStr("playguard/toast/diag_saved", path));
        return true;
    });

    // Saving a backup only writes to the SD card: also offered in read-only mode.
    backup_note->setText(brls::getStr("playguard/tools/backup_note", paths::backups_dir()));
    backup_save->registerClickAction([this](brls::View*) {
        backup_flow::save_now();
        this->refresh();
        return true;
    });
    backup_restore->registerClickAction([this](brls::View*) {
        if (ui::refuse_read_only()) return true;
        backup_flow::choose_and_restore([this]() { this->refresh(); });
        return true;
    });

    backup_keep->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        int selected = 0;
        for (size_t i = 0; i < sizeof(config::BACKUP_KEEP) / sizeof(config::BACKUP_KEEP[0]); i++) {
            labels.push_back(keep_text(config::BACKUP_KEEP[i]));
            if (config::BACKUP_KEEP[i] == config::get().backup_keep) selected = (int)i;
        }
        ui::pick("playguard/tools/backup_keep"_i18n, labels, selected, [this](int i) {
            config::get().backup_keep = config::BACKUP_KEEP[i];
            ui::save_config();
            this->refresh();
        });
        return true;
    });

    // On a firmware newer than the checked one, the firmware screen again.
    compat->registerClickAction([](brls::View*) {
        if (fw_gate::needed()) brls::Application::pushActivity(new FirmwareGateActivity());
        else ui::info("playguard/tools/compat_ok_info"_i18n);
        return true;
    });

    // Developer tools.
    dev_mode->init("playguard/dev/mode"_i18n, app::dev_mode(), [](bool on) {
        if (!app::set_dev_mode(on, true)) ui::notify("playguard/toast/config_err"_i18n);
        ui::on_mode_changed();
    });
    dev_read_only->init("playguard/dev/read_only"_i18n, app::read_only(), [this](bool on) {
        if (on || !fw_gate::needed()) {
            app::set_read_only(on);
            ui::on_mode_changed();
            return;
        }
        // Untested firmware: same warning as "Continue at my own risk".
        this->dev_read_only->setOn(true, false);
        ui::confirm_danger(brls::getStr("playguard/fw_gate/risk_body", fw_gate::firmware()),
                           "playguard/fw_gate/risk_confirm"_i18n, []() {
                               app::set_read_only(false);
                               ui::on_mode_changed();
                           });
    });
    dev_report->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new DiagnosticActivity());
        return true;
    });
    dev_pt_block->registerClickAction([](brls::View*) {
        pt_block_flow::open();
        return true;
    });
    dev_gate->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new FirmwareGateActivity());
        return true;
    });
    dev_forget->registerClickAction([](brls::View*) {
        fw_gate::forget();
        ui::notify("playguard/dev/forgotten"_i18n);
        return true;
    });
}

void ToolsTab::refresh()
{
    const auto& cfg = config::get();
    backup_keep->setDetailText(keep_text(cfg.backup_keep));
    const size_t backups = backup_flow::count();
    backup_restore->setDetailText(backups ? brls::getStr("playguard/tools/backup_count", (int)backups) : "");
    const size_t changes = history::load().size();
    history->setDetailText(changes ? brls::getStr("playguard/history/count", (int)changes) : "");

    SysInfo si;
    sysinfo_get(&si);
    char fwv[16];
    sysinfo_version_string(si.hos_version, fwv, sizeof(fwv));
    const bool ro  = app::read_only();
    const bool dev = app::dev_mode();
    dev_mode->setOn(dev, false);
    dev_read_only->setOn(ro, false);
    fw->setDetailText(fwv);
    ams->setDetailText(si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro)
                                    : "playguard/common/unavailable"_i18n);
    NVGcolor c = ui::color_neutral();
    compat->setDetailText(ui::compat_text(si, &c));
    compat->setDetailTextColor(c);

    storage->setDetailText(ui::storage_text(si));
    c = ui::color_neutral();
    blank->setDetailText(ui::blank_text(si, &c));
    blank->setDetailTextColor(c);
    const bool maskable = si.serial_valid && si.serial[0] && !(si.blank_valid && si.blank);
    serial->setDetailText(ui::serial_text(si, this->serial_revealed));
    serial->setDetailTextColor(ui::serial_warning(si) ? ui::color_warn() : ui::color_neutral());
    serial->setActionAvailable(brls::BUTTON_A, maskable);
    serial->updateActionHint(brls::BUTTON_A, this->serial_revealed ? "playguard/tools/serial_hide"_i18n
                                                                   : "playguard/tools/serial_show"_i18n);
    brls::Application::getGlobalHintsUpdateEvent()->fire();   // redraw the footer hints

    const patches::Report& report = ui::patch_report();
    c = ui::color_neutral();
    game_patches->setDetailText(ui::patches_text(report, &c));
    game_patches->setDetailTextColor(c);
    bool warn = false;
    const std::string note = ui::patches_note(report, si, &warn);
    patches_note->setText(note);
    patches_note->setTextColor(warn ? ui::color_warn() : ui::color_note());
    ui::set_visible_all({ { serial_note.getView(), ui::serial_warning(si) },
                          { patches_note.getView(), !note.empty() },
                          { dev_header.getView(), dev },
                          { dev_mode.getView(), dev },
                          { dev_read_only.getView(), dev },
                          { dev_report.getView(), dev },
                          { dev_pt_block.getView(), dev },
                          { dev_gate.getView(), dev && fw_gate::needed() },
                          { dev_forget.getView(), dev } });
    // Read-only: restoring would write; it stays in sight, greyed.
    ui::show_writable(backup_restore, !ro);
}

brls::View* ToolsTab::create()
{
    return new ToolsTab();
}
