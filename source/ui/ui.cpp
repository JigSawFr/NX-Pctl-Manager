// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include <ctime>
#include <fmt/format.h>
#include <memory>

using namespace brls::literals;

namespace ui
{

NVGcolor color_ok()      { return nvgRGB(46, 184, 108); }
NVGcolor color_warn()    { return nvgRGB(230, 150, 30); }
NVGcolor color_bad()     { return nvgRGB(230, 70, 70); }
NVGcolor color_neutral() { return brls::Application::getTheme()["brls/list/listItem_value_color"]; }
NVGcolor color_text()    { return brls::Application::getTheme()["brls/text"]; }

static std::string hint_for(Result rc)
{
    if (NXM_IS_APP_RESULT(rc)) {
        switch (NXM_RESULT_DESC(rc)) {
            case 1: return "nx_pctl/error/read_only"_i18n;
            case 2: return "nx_pctl/error/write_gated"_i18n;
            case 3: return "nx_pctl/error/fw_unsupported"_i18n;
            case 4: return "nx_pctl/error/unlock_not_effective"_i18n;
            case 5: return "nx_pctl/error/not_custom"_i18n;
            case 6: return "nx_pctl/error/invalid_argument"_i18n;
            case 7: return "nx_pctl/error/autocorrect_off"_i18n;
            case 8: return "nx_pctl/error/state_unknown"_i18n;
            default: return "";
        }
    }
    switch (rc) {
        case 0xF601: return "nx_pctl/error/session_closed"_i18n;
        case 0xF80E: return "nx_pctl/error/bad_pin_format"_i18n;
        default: break;
    }
    if ((rc & 0x1FF) == 142) return "nx_pctl/error/pctl_refused"_i18n;   // pctl module
    return "";
}

std::string rc_text(Result rc)
{
    std::string hint = hint_for(rc);
    std::string code = fmt::format("0x{:08X}", (unsigned)rc);
    return hint.empty() ? brls::getStr("nx_pctl/error/code", code)
                        : brls::getStr("nx_pctl/error/code_hint", code, hint);
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

void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no)
{
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(confirm_label, [on_yes]() { if (on_yes) on_yes(); });
    dialog->setCancelable(true);
    dialog->open();
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
        header, "nx_pctl/numpad/guide"_i18n, 4, std::to_string(current));
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
    if (m == PT_DAY_NOLIMIT) return "nx_pctl/common/no_limit"_i18n;
    if (m == 0) return "nx_pctl/common/zero_minutes"_i18n;
    if (m < 60) return brls::getStr("nx_pctl/common/minutes", (int)m);
    if (m % 60 == 0) return brls::getStr("nx_pctl/common/hours", (int)(m / 60));
    return brls::getStr("nx_pctl/common/hours_minutes", (int)(m / 60), fmt::format("{:02d}", (int)(m % 60)));
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
    return brls::getStr(fmt::format("nx_pctl/days/{}", day));
}

std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no)
{
    if (!ok) return "nx_pctl/common/unavailable"_i18n;
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
            return "nx_pctl/compat/ok"_i18n;
        case SysCompat_UntestedNewer:
            if (color) *color = color_warn();
            return brls::getStr("nx_pctl/compat/untested", std::string(tested));
        case SysCompat_PlayTimerUnsupported:
            if (color) *color = color_warn();
            return "nx_pctl/compat/too_old"_i18n;
        default:
            if (color) *color = color_bad();
            return "nx_pctl/compat/not_ams"_i18n;
    }
}

std::string level_name(uint32_t level)
{
    if (level > 4) return "?";
    return brls::getStr(fmt::format("nx_pctl/restrictions/levels/{}", level));
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
    // Never leave the focus on a view that just disappeared.
    if (had_focus && view->getParent()) brls::Application::giveFocus(view->getParent());
}

}   // namespace ui
