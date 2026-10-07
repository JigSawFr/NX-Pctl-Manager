// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "app.hpp"

#include <ctime>
#include <fmt/format.h>
#include <memory>

using namespace brls::literals;

namespace ui
{

void register_theme_colors()
{
    // PlayGuard brand teal / amber, darkened on the light theme so every status
    // value keeps a contrast of at least 4.5:1 on the borealis backgrounds.
    auto& light = brls::Theme::getLightTheme();
    light.addColor("brand/ok", nvgRGB(0x0A, 0x6E, 0x5C));
    light.addColor("brand/warn", nvgRGB(0x8A, 0x52, 0x00));
    light.addColor("brand/bad", nvgRGB(0xB7, 0x1C, 0x1C));
    light.addColor("brand/gauge_track", nvgRGBA(0, 0, 0, 34));
    light.addColor("brand/note", nvgRGB(0x5C, 0x5C, 0x5C));
    auto& dark = brls::Theme::getDarkTheme();
    dark.addColor("brand/ok", nvgRGB(0x2E, 0xC4, 0xA6));
    dark.addColor("brand/warn", nvgRGB(0xFF, 0xB5, 0x47));
    dark.addColor("brand/bad", nvgRGB(0xFF, 0x7A, 0x7A));
    dark.addColor("brand/gauge_track", nvgRGBA(255, 255, 255, 46));
    dark.addColor("brand/note", nvgRGB(0xB8, 0xB8, 0xB8));
}

NVGcolor color_ok()      { return brls::Application::getTheme()["brand/ok"]; }
NVGcolor color_warn()    { return brls::Application::getTheme()["brand/warn"]; }
NVGcolor color_bad()     { return brls::Application::getTheme()["brand/bad"]; }
NVGcolor color_track()   { return brls::Application::getTheme()["brand/gauge_track"]; }
NVGcolor color_note()    { return brls::Application::getTheme()["brand/note"]; }
NVGcolor color_neutral() { return brls::Application::getTheme()["brls/list/listItem_value_color"]; }
NVGcolor color_text()    { return brls::Application::getTheme()["brls/text"]; }

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

std::string rc_text(Result rc)
{
    std::string hint = hint_for(rc);
    std::string code = fmt::format("0x{:08X}", (unsigned)rc);
    return hint.empty() ? brls::getStr("playguard/error/code", code)
                        : brls::getStr("playguard/error/code_hint", code, hint);
}

void notify(const std::string& text)
{
    brls::sync([text]() { brls::Application::notify(text); });
}

void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix)
{
    if (R_SUCCEEDED(rc)) notify(ok_text);
    else notify(error_prefix + " — " + rc_text(rc));
}

static void open_confirm(const std::string& body, const std::string& confirm_label,
                         std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(confirm_label, [on_yes]() { if (on_yes) on_yes(); });
    dialog->setCancelable(true);
    if (danger)
        if (auto* button = dynamic_cast<brls::Button*>(dialog->getView("brls/dialog/button2")))
            button->setTextColor(color_bad());
    dialog->open();
}

void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), false);
}

void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes)
{
    open_confirm(body, confirm_label, std::move(on_yes), nullptr, true);
}

void info(const std::string& body)
{
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick)
{
    if (values.empty()) return;
    auto chosen = std::make_shared<int>(-1);
    auto* dropdown = new brls::Dropdown(
        title, values, [chosen](int index) { *chosen = index; },
        selected < 0 ? 0 : selected,
        [chosen, on_pick](int) {
            if (*chosen >= 0 && on_pick) {
                int index = *chosen;
                brls::sync([on_pick, index]() { on_pick(index); });
            }
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void prompt_minutes(const std::string& header, uint16_t current, std::function<void(uint16_t)> on_value)
{
    brls::Application::getImeManager()->openForNumber(
        [on_value](long v) {
            if (v < 0) v = 0;
            if (v > 1440) v = 1440;
            on_value((uint16_t)v);
        },
        header, "playguard/numpad/guide"_i18n, 4, std::to_string(current));
}

void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value)
{
    brls::Application::getImeManager()->openForText(
        [on_value](std::string text) { if (!text.empty()) on_value(text); },
        header, "", max_len, initial);
}

std::string fmt_minutes(uint16_t m)
{
    if (m == PT_DAY_NOLIMIT) return "playguard/common/no_limit"_i18n;
    if (m == 0) return "playguard/common/zero_minutes"_i18n;
    if (m < 60) return brls::getStr("playguard/common/minutes", (int)m);
    if (m % 60 == 0) return brls::getStr("playguard/common/hours", (int)(m / 60));
    return brls::getStr("playguard/common/hours_minutes", (int)(m / 60), fmt::format("{:02d}", (int)(m % 60)));
}

std::string fmt_duration_ns(uint64_t ns)
{
    uint64_t minutes = (ns + 30000000000ULL) / 60000000000ULL;
    if (minutes > 1440) minutes = 1440;
    return fmt_minutes((uint16_t)minutes);
}

std::string day_name(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playguard/days/{}", day));
}

std::string day_name_in_text(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playguard/days_lower/{}", day));
}

std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no)
{
    if (!ok) return "playguard/common/unavailable"_i18n;
    return value ? yes : no;
}

int today_weekday()
{
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    return tmv.tm_wday;
}

std::string fw_text(const SysInfo& info)
{
    char fw[16];
    sysinfo_version_string(info.hos_version, fw, sizeof(fw));
    std::string ams = info.ams_valid ? fmt::format("{}.{}.{}", info.ams_major, info.ams_minor, info.ams_micro)
                                     : std::string("?");
    return fmt::format("{} · Atmosphère {}", fw, ams);
}

std::string compat_text(const SysInfo& info, NVGcolor* color)
{
    char tested[16];
    sysinfo_version_string(PCTL_FW_TESTED_MAX, tested, sizeof(tested));
    switch (sysinfo_compat(&info)) {
        case SysCompat_Ok:
            if (color) *color = color_ok();
            return "playguard/compat/ok"_i18n;
        case SysCompat_UntestedNewer:
            if (color) *color = color_warn();
            return brls::getStr("playguard/compat/untested", std::string(tested));
        case SysCompat_PlayTimerUnsupported:
            if (color) *color = color_warn();
            return "playguard/compat/too_old"_i18n;
        default:
            if (color) *color = color_bad();
            return "playguard/compat/not_ams"_i18n;
    }
}

std::string level_name(uint32_t level)
{
    if (level > 4) return "?";
    return brls::getStr(fmt::format("playguard/restrictions/levels/{}", level));
}

std::string time_text(uint64_t posix)
{
    if (!posix) return "—";
    char buf[48];
    time_format_local(posix, buf, sizeof(buf));
    return buf;
}

void set_visible(brls::View* view, bool visible)
{
    if (!view) return;
    const bool had_focus = !visible && view->isFocused();
    view->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!had_focus) return;

    // Never leave the focus on a view that just disappeared: prefer the
    // nearest focusable sibling, then the container.
    brls::Box* parent = view->getParent();
    if (!parent) return;
    const auto& children = parent->getChildren();
    const int count = (int)children.size();
    int index = -1;
    for (int i = 0; i < count; i++)
        if (children[i] == view) index = i;
    for (int distance = 1; index >= 0 && distance < count; distance++) {
        for (int i : { index + distance, index - distance }) {
            if (i < 0 || i >= count) continue;
            if (brls::View* target = children[i]->getDefaultFocus()) {
                brls::Application::giveFocus(target);
                return;
            }
        }
    }
    brls::Application::giveFocus(parent);
}

void set_visible_all(std::initializer_list<std::pair<brls::View*, bool>> changes)
{
    for (const auto& c : changes)
        if (c.second) set_visible(c.first, true);
    for (const auto& c : changes)
        if (!c.second) set_visible(c.first, false);
}

void go_to_tab(brls::View* from, int position)
{
    brls::TabFrame* frame = nullptr;
    for (brls::View* v = from; v && !frame; v = v->getParent())
        frame = dynamic_cast<brls::TabFrame*>(v);
    if (!frame) return;
    brls::sync([frame, position]() {
        frame->focusTab(position);   // replaces the tab content
        const auto& children = frame->getChildren();
        if (children.size() >= 2) brls::Application::giveFocus(children.back());
    });
}

void init_unlock_banner(brls::DetailCell* cell, std::function<void()> after)
{
    cell->setText("playguard/security/banner_title"_i18n);
    cell->title->setTextColor(color_warn());
    cell->setBackgroundColor(nvgTransRGBA(color_warn(), 28));
    cell->setCornerRadius(6);
    if (app::read_only_build()) {
        // No action: give the title the whole width (an empty detail label
        // still reserves its space).
        cell->detail->setVisibility(brls::Visibility::GONE);
        return;
    }
    cell->setDetailText("playguard/security/banner_action"_i18n);
    cell->registerClickAction([after](brls::View*) {
        Result rc = pctl_relock();
        notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        if (after) after();
        return true;
    });
}

void show_unlock_banner(brls::DetailCell* cell, bool unlocked)
{
    set_visible(cell, unlocked);
}

void offer_restart()
{
    auto* dialog = new brls::Dialog("playguard/common/restart_body"_i18n);
    dialog->addButton("playguard/common/later"_i18n, []() {});
    dialog->addButton("playguard/common/quit"_i18n, []() { brls::Application::quit(); });
    dialog->setCancelable(true);
    dialog->open();
}

}   // namespace ui
