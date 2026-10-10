// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/dev_build_flow.hpp"

#include <borealis.hpp>
#include <cstdio>
#include <ctime>
#include <memory>
#include <vector>

#include "action/github_login_flow.hpp"
#include "action/pin_lock.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/dev_builds.hpp"
#include "util/github_auth.hpp"
#include "util/launcher.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace dev_build_flow
{

namespace
{
bool s_busy = false;   // one list or download at a time

// The last list fetched (dev_builds::Cache), read from the SD card on the
// first opening of this run.
dev_builds::Cache s_cache;
bool              s_cache_read = false;

std::string cache_file() { return paths::data_dir() + "/cache/dev_builds.json"; }

void read_cache()
{
    if (s_cache_read) return;
    s_cache_read = true;
    std::string text;
    if (paths::read_file(cache_file(), text) && !dev_builds::decode_cache(text, &s_cache)) s_cache = {};
}

void keep(const dev_builds::Cache& cache)
{
    s_cache = cache;
    std::string err;
    // Only saves time on the next start: a write that fails is just logged.
    if (!paths::ensure_dir(paths::data_dir() + "/cache") ||
        !paths::atomic_write(cache_file(), dev_builds::encode_cache(cache), &err))
        brls::Logger::warning("dev builds: cannot keep the list ({})", err);
}

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

void show(const dev_builds::Cache& cache)
{
    std::vector<std::string> labels;
    int selected = 0;
    for (size_t i = 0; i < cache.builds.size(); i++) {
        labels.push_back(label(cache.builds[i]));
        if (installed(cache.builds[i])) selected = (int)i;
    }
    // Without a token GitHub hands out the release only: a line signs in.
    const int sign_in = cache.needs_login ? (int)labels.size() : -1;
    if (cache.needs_login) labels.push_back("playguard/dev_build/sign_in"_i18n);
    const int refresh = (int)labels.size();
    const int64_t age = (int64_t)std::time(nullptr) - cache.fetched_at;
    labels.push_back(brls::getStr("playguard/dev_build/refresh", (int)(age > 0 ? age / 60 : 0)));
    auto list = std::make_shared<std::vector<dev_builds::Build>>(cache.builds);
    ui::pick("playguard/dev_build/pick"_i18n, labels, selected, [list, sign_in, refresh](int i) {
        if (i == refresh) {
            open(true);
            return;
        }
        if (i == sign_in) {
            github_login_flow::sign_in([]() { open(); });
            return;
        }
        if (i < 0 || (size_t)i >= list->size()) return;
        const dev_builds::Build b = (*list)[(size_t)i];
        std::string running = app::version();
        if (!app::commit().empty()) running += " (" + app::commit() + ")";
        std::string body = brls::getStr("playguard/dev_build/confirm", running, label(b));
        if (b.sha256.empty()) body += "\n\n" + "playguard/dev_build/no_digest"_i18n;
        ui::confirm(body, "playguard/dev_build/install"_i18n, [b]() {
            // Replacing PlayGuard can undo everything it guards: the PIN is
            // asked whenever one is set, whatever Security › Ask for the PIN says.
            if (!pin_lock::ask()) {
                ui::notify(pin_lock::refusal_text());
                return;
            }
            install(b);
        });
    });
}
}   // namespace

void open(bool force)
{
    if (s_busy) {
        ui::notify("playguard/dev_build/busy"_i18n);
        return;
    }
    // A list fetched a few minutes ago, in the same sign-in state, is shown at
    // once; an older one is fetched again (and still shown if GitHub cannot
    // be reached).
    read_cache();
    const bool needs_login = github_auth::token().empty();
    if (!force && dev_builds::cache_fresh(s_cache, (int64_t)std::time(nullptr), needs_login)) {
        show(s_cache);
        return;
    }
    s_busy = true;
    ui::notify("playguard/dev_build/loading"_i18n);
    // Not behind the play log or the game icons (ui::in_background).
    ui::in_background("dev build list", []() {
        dev_builds::Cache fresh;
        std::string err;
        const bool ok = dev_builds::fetch(&fresh.builds, &fresh.needs_login, &err);
        fresh.fetched_at = (int64_t)std::time(nullptr);
        brls::sync([ok, fresh, err]() {
            s_busy = false;
            if (ok) {
                keep(fresh);
                show(fresh);
            } else if (!s_cache.builds.empty()) {
                ui::notify(brls::getStr("playguard/dev_build/list_stale", err));
                show(s_cache);
            } else if (fresh.needs_login) {
                show(fresh);   // signing in may still list them
            } else {
                ui::info(brls::getStr("playguard/dev_build/list_failed", err));
            }
        });
    });
}

}   // namespace dev_build_flow
