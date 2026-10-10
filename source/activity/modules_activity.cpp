// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/modules_activity.hpp"

#include <cstdlib>
#include <fmt/format.h>

#include "action/agent_update.hpp"
#include "action/history_flow.hpp"
#include "action/pin_lock.hpp"
#include "action/sync_flow.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace
{
std::string key(const modules::Module& m, const char* what)
{
    return std::string("playguard/modules/") + m.name + "/" + what;
}

std::string short_sha(const std::string& sha)
{
    return sha.size() > 12 ? sha.substr(0, 12) : sha;
}

// The PIN, as for a console change; says why not when refused.
bool allowed()
{
    if (pin_lock::allow_change()) return true;
    ui::notify(pin_lock::refusal_text());
    return false;
}

void record(const modules::Module& m, const char* what)
{
    history_flow::record_event("module", "", brls::getStr(std::string("playguard/modules/done/") + what, m.title));
}
}   // namespace

ModulesActivity::~ModulesActivity()
{
    *alive = false;
}

std::string ModulesActivity::bundle_dir()
{
#ifndef __SWITCH__
    if (const char* sim = std::getenv("PLAYGUARD_SIM_BUNDLED")) return sim;
#endif
    return BRLS_ASSET("sysmodules");
}

std::string ModulesActivity::summary(modules::Id id)
{
    const modules::Module& m = modules::get(id);
    const modules::State s = modules::state(paths::sd_root(), m);
    if (!s.installed) return "playguard/modules/state/not_installed"_i18n;
    if (modules::offer(s, modules::bundled(bundle_dir(), m)) == modules::Offer::Update)
        return "playguard/modules/state/update"_i18n;
    return "playguard/modules/state/installed"_i18n;
}

void ModulesActivity::onContentAvailable()
{
    intro->setSingleLine(false);
    intro->setText("playguard/modules/intro"_i18n);
    const modules::Id ids[2] = { modules::Id::Rescue, modules::Id::Agent };
    for (int i = 0; i < 2; i++) {
        Row& row = rows[i];
        row.id = ids[i];
        const std::string p = std::string("md_") + modules::get(ids[i]).name + "_";
        row.status = dynamic_cast<brls::DetailCell*>(this->getView(p + "status"));
        row.action = dynamic_cast<brls::DetailCell*>(this->getView(p + "action"));
        row.boot = dynamic_cast<brls::BooleanCell*>(this->getView(p + "boot"));
        row.run = modules::get(ids[i]).resident ? dynamic_cast<brls::DetailCell*>(this->getView(p + "run")) : nullptr;
        row.remove = dynamic_cast<brls::DetailCell*>(this->getView(p + "remove"));
        if (auto* about = dynamic_cast<brls::Label*>(this->getView(p + "about"))) about->setSingleLine(false);
        this->bind(row);
    }
    this->refresh();
}

void ModulesActivity::bind(Row& row)
{
    const modules::Id id = row.id;
    row.status->registerClickAction([id](brls::View*) {
        const modules::Module& m = modules::get(id);
        const modules::State s = modules::state(paths::sd_root(), m);
        const modules::Bundle b = modules::bundled(bundle_dir(), m);
        std::string text = brls::getStr(key(m, "title")) + "\n";
        text += "\n" + brls::getStr("playguard/modules/details/where", modules::dir(paths::sd_root(), m));
        if (s.installed)
            text += "\n" + brls::getStr("playguard/modules/details/installed",
                                        s.version.empty() ? "playguard/modules/details/by_hand"_i18n : s.version,
                                        short_sha(s.sha256));
        text += "\n" + (b.present ? brls::getStr("playguard/modules/details/bundled", b.version, short_sha(b.sha256))
                                  : "playguard/modules/details/not_bundled"_i18n);
        ui::info(text);
        return true;
    });
    row.action->registerClickAction([this, id](brls::View*) {
        for (const Row& r : rows)
            if (r.id == id) this->install(r);
        return true;
    });
    row.boot->init("playguard/modules/at_boot"_i18n, false, [this, id](bool on) {
        const modules::Module& m = modules::get(id);
        std::string err;
        if (!allowed() || !modules::set_at_boot(paths::sd_root(), m, on, &err)) {
            if (!err.empty()) ui::error("playguard/modules/error"_i18n + " — " + err);
        } else {
            record(m, on ? "boot_on" : "boot_off");
        }
        this->refresh();
    });
    if (row.run) {
        row.run->registerClickAction([this, id](brls::View*) {
            const modules::Module& m = modules::get(id);
            Result rc = 0;
            const bool running = module_running(m.tid, &rc);
            if (!allowed()) return true;
            if (running) sync_flow::agent_stopping();
            rc = running ? module_terminate(m.tid) : module_launch(m.tid);
            ui::notify_result(rc, running ? "playguard/modules/stopped"_i18n : "playguard/modules/started"_i18n,
                              "playguard/modules/error"_i18n);
            if (R_SUCCEEDED(rc)) record(m, running ? "stopped" : "started");
            // The link follows: the agent's, or PlayGuard's own.
            if (running || R_FAILED(rc)) sync_flow::reload();
            else sync_flow::agent_started();
            this->refresh();
            return true;
        });
    }
    row.remove->registerClickAction([this, id](brls::View*) {
        for (const Row& r : rows)
            if (r.id == id) this->uninstall(r);
        return true;
    });
}

void ModulesActivity::install(const Row& row)
{
    const modules::Module& m = modules::get(row.id);
    const modules::Bundle b = modules::bundled(bundle_dir(), m);
    if (!b.present) {
        ui::info("playguard/modules/details/not_bundled"_i18n);
        return;
    }
    const modules::State s = modules::state(paths::sd_root(), m);
    const bool update = s.installed;
    const std::string body = brls::getStr(key(m, update ? "update_body" : "install_body"), b.version);
    const modules::Id id = row.id;
    ui::confirm(body, update ? "playguard/modules/update"_i18n : "playguard/modules/install"_i18n, [this, id, b, update]() {
        const modules::Module& m = modules::get(id);
        if (!allowed()) return;
        if (m.resident) {
            // The agent: stopped cleanly, swapped, started and seen answering,
            // or the previous one put back (action/agent_update).
            if (agent_update::running()) {
                ui::notify("playguard/modules/agent/busy"_i18n);
                return;
            }
            ui::notify(update ? "playguard/modules/agent/updating"_i18n : "playguard/modules/agent/installing"_i18n);
            std::shared_ptr<bool> alive = this->alive;
            agent_update::run(bundle_dir(), [this, alive](const agent_update::Outcome& o) {
                agent_update::tell(o);
                if (*alive) this->refresh();
            });
            return;
        }
        // The recovery module only acts at boot: nothing to stop or start.
        const std::string sd = paths::sd_root();
        std::string err;
        if (!modules::install(sd, m, b, false, &err)) {
            ui::error("playguard/modules/error"_i18n + " — " + err);
            this->refresh();
            return;
        }
        modules::confirm(sd, m);
        record(m, update ? "updated" : "installed");
        ui::notify(brls::getStr(key(m, update ? "updated" : "installed")));
        this->refresh();
    });
}

void ModulesActivity::uninstall(const Row& row)
{
    const modules::Module& m = modules::get(row.id);
    if (!modules::state(paths::sd_root(), m).installed) return;
    const modules::Id id = row.id;
    ui::confirm_danger(brls::getStr(key(m, "remove_body")), "playguard/modules/remove"_i18n, [this, id]() {
        const modules::Module& m = modules::get(id);
        if (!allowed()) return;
        if (m.resident && module_running(m.tid, nullptr)) {
            sync_flow::agent_stopping();
            module_terminate(m.tid);
            sync_flow::reload();
        }
        std::string err;
        if (!modules::uninstall(paths::sd_root(), m, &err)) {
            ui::error("playguard/modules/error"_i18n + " — " + err);
        } else {
            record(m, "removed");
            ui::notify("playguard/modules/removed"_i18n);
        }
        this->refresh();
    });
}

void ModulesActivity::refresh()
{
    const std::string sd = paths::sd_root();
    for (const Row& row : rows) {
        const modules::Module& m = modules::get(row.id);
        const modules::State s = modules::state(sd, m);
        const modules::Bundle b = modules::bundled(bundle_dir(), m);
        const modules::Offer offer = modules::offer(s, b);
        bool running = false;
        if (m.resident && s.installed) running = module_running(m.tid, nullptr);

        std::string status;
        NVGcolor color = ui::color_neutral();
        if (!s.installed) {
            status = b.present ? "playguard/modules/state/not_installed"_i18n : "playguard/modules/state/not_bundled"_i18n;
        } else if (offer == modules::Offer::Update) {
            status = "playguard/modules/state/update"_i18n;
            color = ui::color_warn();
        } else {
            status = "playguard/modules/state/installed"_i18n;
            color = ui::color_ok();
        }
        if (m.resident && s.installed)
            status += " · " + (running ? "playguard/modules/state/running"_i18n : "playguard/modules/state/stopped"_i18n);
        row.status->setDetailText(status);
        row.status->setDetailTextColor(color);

        row.action->setText(offer == modules::Offer::Update ? "playguard/modules/update"_i18n : "playguard/modules/install"_i18n);
        row.action->setDetailText(b.present ? b.version : "");
        ui::set_visible(row.action, offer != modules::Offer::None);
        row.boot->setOn(s.at_boot, false);
        ui::set_visible(row.boot, s.installed);
        if (row.run) {
            row.run->setText(running ? "playguard/modules/stop"_i18n : "playguard/modules/start"_i18n);
            ui::set_visible(row.run, s.installed);
        }
        ui::set_visible(row.remove, s.installed);
    }
}
