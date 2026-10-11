// ui — result texts, background tasks, toasts, screen changes and the main
// screen's state. The other ui/*.cpp hold the rest of ui.hpp by concern.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "action/pt_log_flow.hpp"
#include "activity/main_activity.hpp"
#include "tab/tab_base.hpp"
#include "util/config.hpp"

#include <atomic>
#include <condition_variable>
#include <fmt/format.h>
#include <mutex>
#include <system_error>
#include <thread>

using namespace brls::literals;

namespace ui
{

static std::string hint_for(Result rc)
{
    if (NXM_IS_APP_RESULT(rc)) {
        switch (NXM_RESULT_DESC(rc)) {
            case 1: return "playguard/error/read_only"_i18n;
            case 2: return "playguard/error/write_gated"_i18n;
            case 3: return "playguard/error/fw_unsupported"_i18n;
            case 4: return "playguard/error/unlock_not_effective"_i18n;
            case 5: return "playguard/error/not_custom"_i18n;
            case 6: return "playguard/error/invalid_argument"_i18n;
            case 7: return "playguard/error/autocorrect_off"_i18n;
            case 8: return "playguard/error/state_unknown"_i18n;
            case 9: return "playguard/error/not_confirmed"_i18n;
            case 10: return "playguard/error/no_pin"_i18n;
            case 11: return "playguard/error/relock_failed"_i18n;
            case 12: return "playguard/error/not_applied"_i18n;
            case 13: return "playguard/error/not_saved"_i18n;
            case 14: return "playguard/error/pt_not_understood"_i18n;
            default: return "";
        }
    }
    switch (rc) {
        case 0xF601: return "playguard/error/session_closed"_i18n;
        case 0xF80E: return "playguard/error/bad_pin_format"_i18n;
        default: break;
    }
    if ((rc & 0x1FF) == 142) return "playguard/error/pctl_refused"_i18n;   // pctl module
    return "";
}

std::string rc_text(Result rc, bool next_step)
{
    std::string hint = hint_for(rc);
    if (NXM_IS_APP_RESULT(rc) && !hint.empty()) {
        if (hint[0] >= 'a' && hint[0] <= 'z') hint[0] = (char)(hint[0] - 'a' + 'A');
        return hint;
    }
    std::string code = fmt::format("0x{:08X}", (unsigned)rc);
    std::string text = hint.empty() ? brls::getStr("playguard/error/code", code)
                                    : brls::getStr("playguard/error/code_hint", code, hint);
    // A system code alone leaves the parent stuck: say what to do next. App
    // results already explain themselves.
    if (next_step && !NXM_IS_APP_RESULT(rc)) text += "\n\n" + "playguard/error/next_step"_i18n;
    return text;
}

namespace
{
std::mutex s_bg_lock;
std::condition_variable s_bg_done;
int s_bg_running = 0;   // tasks of in_background on their own thread
std::atomic<bool> s_quitting{ false };
}   // namespace

void in_background(const char* what, std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(s_bg_lock);
        s_bg_running++;
    }
    try {
        std::thread([task]() {
            task();
            std::lock_guard<std::mutex> lock(s_bg_lock);
            s_bg_running--;
            s_bg_done.notify_all();
        }).detach();
    } catch (const std::system_error& e) {
        {
            std::lock_guard<std::mutex> lock(s_bg_lock);
            s_bg_running--;
        }
        brls::Logger::warning("{}: no thread ({}), queued instead", what, e.what());
        brls::async(task);
    }
}

bool quitting()
{
    return s_quitting;
}

bool finish_background(std::chrono::milliseconds max)
{
    s_quitting = true;
    std::unique_lock<std::mutex> lock(s_bg_lock);
    const bool done = s_bg_done.wait_for(lock, max, []() { return s_bg_running == 0; });
    if (!done) brls::Logger::warning("quitting with {} background task(s) still running", s_bg_running);
    return done;
}

void notify(const std::string& text)
{
    // Logged too: the desktop smoke test checks what the user was told.
    brls::Logger::info("toast: {}", text);
    brls::sync([text]() { brls::Application::notify(text); });
}

void error(const std::string& text)
{
    brls::Logger::info("error: {}", text);
    brls::sync([text]() { info(text); });
}

void replace_screen(brls::Activity* next)
{
    const auto none = brls::TransitionAnimation::NONE;
    if (!brls::Application::popActivity(none, [next, none]() { brls::Application::pushActivity(next, none); }))
        brls::Application::pushActivity(next, none);
}

bool save_config()
{
    if (config::save()) return true;
    notify("playguard/toast/config_err"_i18n);
    return false;
}

void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix)
{
    if (R_SUCCEEDED(rc)) notify(ok_text);
    else error(error_prefix + " — " + rc_text(rc));
}

static bool s_unlocked = false;

bool known_unlocked() { return s_unlocked; }

void note_unlocked(bool valid, bool unlocked)
{
    if (!valid || unlocked == s_unlocked) return;
    s_unlocked = unlocked;
    // Next frame: at start-up the first tab is read before the main screen is
    // on the activity stack.
    brls::sync([]() {
        for (brls::Activity* activity : brls::Application::getActivitiesStack())
            if (auto* main = dynamic_cast<MainActivity*>(activity)) main->update_title();
    });
}

void on_mode_changed()
{
    pt_log_flow::apply();   // the recorder only runs in developer mode
    for (brls::Activity* activity : brls::Application::getActivitiesStack())
        if (auto* main = dynamic_cast<MainActivity*>(activity)) main->update_title();
    TabBase::refresh_shown();
}

}   // namespace ui
