// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/dev_build_flow.hpp"

#include <borealis.hpp>
#include <cstdio>
#include <memory>
#include <vector>

#include "action/github_login_flow.hpp"
#include "action/pin_lock.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/dev_builds.hpp"
#include "util/launcher.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace dev_build_flow
{

namespace
{
bool s_busy = false;   // one list or download at a time

// The .nro to replace, from the SD card root: the running one. On desktop,
// where nothing runs from an SD card, the usual place on the simulated one.
std::string sd_path()
{
#ifdef __SWITCH__
    return app::self_path();
#else
    return "/switch/playguard/playguard.nro";
#endif
}

// That file for the C library ("/switch/…" on the console, under
// ./playguard_data/sd on desktop).
std::string file_path(const std::string& sd)
{
    const std::string root = paths::sd_root();
    return root == "/" ? sd : root + sd;
}

// "2026-10-09T12:03:00Z" -> "2026-10-09 12:03"
std::string short_date(const std::string& iso)
{
    if (iso.size() < 16) return iso;
    return iso.substr(0, 10) + " " + iso.substr(11, 5);
}

bool installed(const dev_builds::Build& b)
{
    if (b.kind == dev_builds::Kind::Release) return false;   // a release is told by its version, below
    return !app::commit().empty() && b.commit == app::commit();
}

std::string label(const dev_builds::Build& b)
{
    std::string text;
    switch (b.kind) {
        case dev_builds::Kind::Release:
            text = brls::getStr("playguard/dev_build/release", b.version);
            break;
        case dev_builds::Kind::Main:
            text = brls::getStr("playguard/dev_build/main", b.commit, short_date(b.date));
            break;
        case dev_builds::Kind::PullRequest:
            text = brls::getStr("playguard/dev_build/pr", b.pr, b.title, b.commit);
            break;
    }
    if (installed(b)) text += " · " + "playguard/dev_build/installed"_i18n;
    return text;
}

void installed_now(const dev_builds::Build& b)
{
    if (launcher::can_launch() && launcher::launch_nro(sd_path())) {
        // hbloader starts the new .nro as soon as this one exits.
        brls::Application::quit();
        return;
    }
    ui::info(brls::getStr("playguard/dev_build/done_manual", label(b)));
}

void install(const dev_builds::Build& b)
{
    if (sd_path().empty()) {
        ui::info("playguard/dev_build/no_path"_i18n);
        return;
    }
    if (s_busy) return;
    s_busy = true;
    ui::notify(brls::getStr("playguard/dev_build/downloading", (int)((b.size + 512 * 1024) / (1024 * 1024))));
    const std::string target = file_path(sd_path());
    // Not behind the play log or the game icons (ui::in_background).
    ui::in_background("dev build download", [b, target]() {
        std::string err;
        const std::string fresh = target + ".new";
        bool ok = dev_builds::download(b, fresh, &err);
        if (ok) {
            ok = dev_builds::replace(target, fresh, &err);
            if (!ok) std::remove(fresh.c_str());
        }
        brls::sync([b, ok, err]() {
            s_busy = false;
            if (!ok) ui::info(brls::getStr("playguard/dev_build/failed", err));
            else installed_now(b);
        });
    });
}

void show(std::vector<dev_builds::Build> builds, bool needs_login)
{
    std::vector<std::string> labels;
    int selected = 0;
    for (size_t i = 0; i < builds.size(); i++) {
        labels.push_back(label(builds[i]));
        if (installed(builds[i])) selected = (int)i;
    }
    // Without a token GitHub hands out the release only: the last line signs in.
    if (needs_login) labels.push_back("playguard/dev_build/sign_in"_i18n);
    auto list = std::make_shared<std::vector<dev_builds::Build>>(std::move(builds));
    ui::pick("playguard/dev_build/pick"_i18n, labels, selected, [list](int i) {
        if ((size_t)i >= list->size()) {
            github_login_flow::sign_in([]() { open(); });
            return;
        }
        const dev_builds::Build b = (*list)[(size_t)i];
        std::string running = app::version();
        if (!app::commit().empty()) running += " (" + app::commit() + ")";
        std::string body = brls::getStr("playguard/dev_build/confirm", running, label(b));
        if (b.sha256.empty()) body += "\n\n" + "playguard/dev_build/no_digest"_i18n;
        ui::confirm(body, "playguard/dev_build/install"_i18n, [b]() {
            // Replacing PlayGuard is a change: Security › Ask for the PIN applies.
            if (config::get().pin_lock != "off" && !pin_lock::ask()) return;
            install(b);
        });
    });
}
}   // namespace

void open()
{
    if (s_busy) {
        ui::notify("playguard/dev_build/busy"_i18n);
        return;
    }
    s_busy = true;
    ui::notify("playguard/dev_build/loading"_i18n);
    ui::in_background("dev build list", []() {
        std::vector<dev_builds::Build> builds;
        std::string err;
        bool needs_login = false;
        const bool ok = dev_builds::fetch(&builds, &needs_login, &err);
        brls::sync([ok, builds, needs_login, err]() {
            s_busy = false;
            if (ok || needs_login) show(builds, needs_login);   // signing in may still list them
            else ui::info(brls::getStr("playguard/dev_build/list_failed", err));
        });
    });
}

}   // namespace dev_build_flow
