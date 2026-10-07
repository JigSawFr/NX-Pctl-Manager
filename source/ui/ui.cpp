// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "app.hpp"
#include "util/duration.hpp"

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
    // A clear green, not the logo teal: the dark theme's default value colour is
    // already teal, so "OK" would not stand out from plain values.
    dark.addColor("brand/ok", nvgRGB(0x7E, 0xD9, 0x57));
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
    const std::string guide   = "playguard/numpad/guide"_i18n;
    const std::string initial = duration::format_hm(current == PT_DAY_NOLIMIT ? 60 : current);
    auto handle = [on_value](const std::string& text) {
        uint16_t minutes = 0;
        if (!duration::parse(text, &minutes)) {
            notify(rc_text(NXM_RC_INVALID_ARGUMENT));
            return;
        }
        on_value(minutes);
    };
#ifdef __SWITCH__
    // The system number pad with a ":" key, so "1:30" can be typed. borealis'
    // openForNumber reads the result with stoll and would stop at the colon.
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetLeftOptionalSymbolKey(&kbd, ":");
    swkbdConfigSetHeaderText(&kbd, header.c_str());
    swkbdConfigSetSubText(&kbd, guide.c_str());
    swkbdConfigSetStringLenMax(&kbd, 5);
    swkbdConfigSetInitialText(&kbd, initial.c_str());
    swkbdConfigSetBlurBackground(&kbd, true);
    char out[16] = {};
    const Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc) && out[0]) handle(out);
#else
    brls::Application::getImeManager()->openForText([handle](std::string text) { handle(text); },
                                                   header, guide, 5, initial);
#endif
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

std::string fmt_play_time(uint64_t seconds)
{
    if (seconds > 0 && seconds < 60) return "playguard/activity/less_than_minute"_i18n;
    const uint64_t m = (seconds + 30) / 60;
    if (m < 60) return brls::getStr("playguard/common/minutes", (int)m);
    if (m % 60 == 0) return brls::getStr("playguard/common/hours", (unsigned long long)(m / 60));
    return brls::getStr("playguard/common/hours_minutes", (unsigned long long)(m / 60), fmt::format("{:02d}", (int)(m % 60)));
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

std::string storage_text(const SysInfo& info)
{
    switch (sysinfo_storage(&info)) {
        case SysStorage_EmuMMC: return "playguard/tools/storage_emummc"_i18n;
        case SysStorage_SysMMC: return "playguard/tools/storage_sysmmc"_i18n;
        default:                return "playguard/tools/storage_unknown"_i18n;
    }
}

std::string storage_short(const SysInfo& info)
{
    switch (sysinfo_storage(&info)) {
        case SysStorage_EmuMMC: return "emuMMC";
        case SysStorage_SysMMC: return "sysMMC";
        default:                return "—";
    }
}

bool serial_warning(const SysInfo& info)
{
    return sysinfo_storage(&info) == SysStorage_EmuMMC && info.blank_valid && !info.blank;
}

std::string blank_text(const SysInfo& info, NVGcolor* color)
{
    if (!info.blank_valid) {
        if (color) *color = color_neutral();
        return "playguard/common/unavailable"_i18n;
    }
    if (color) *color = info.blank ? color_ok() : (serial_warning(info) ? color_warn() : color_neutral());
    return info.blank ? "playguard/common/yes"_i18n : "playguard/common/no"_i18n;
}

std::string serial_text(const SysInfo& info, bool reveal)
{
    if (!info.serial_valid || !info.serial[0]) return "playguard/common/unavailable"_i18n;
    std::string s = info.serial;
    if (info.blank_valid && info.blank) return brls::getStr("playguard/tools/serial_blanked", s);
    if (reveal || s.size() < 8) return s;
    // Keep the prefix (model / region) and the last two digits.
    std::string masked = s.substr(0, 7);
    for (size_t i = 7; i + 2 < s.size(); i++) masked += "•";
    return masked + s.substr(s.size() - 2);
}

bool patches_warning(const patches::Report& report)
{
    const auto st = patches::status(report);
    return st == patches::Status::FilesOnly || st == patches::Status::SysPatchIncomplete;
}

static std::string join_list(const std::vector<std::string>& items)
{
    std::string out;
    for (const auto& i : items) out += (out.empty() ? "" : ", ") + i;
    return out;
}

std::string patches_text(const patches::Report& report, NVGcolor* color)
{
    switch (patches::status(report)) {
        case patches::Status::SysPatch:
            if (color) *color = color_ok();
            return "playguard/tools/patches_syspatch"_i18n;
        case patches::Status::SysPatchIncomplete:
            if (color) *color = color_warn();
            return "playguard/tools/patches_incomplete"_i18n;
        case patches::Status::FilesOnly:
            if (color) *color = color_warn();
            return brls::getStr("playguard/tools/patches_files", join_list(report.files));
        default:
            if (color) *color = color_neutral();
            return "playguard/tools/patches_none"_i18n;
    }
}

std::string patches_note(const patches::Report& report, const SysInfo& info, bool* warn)
{
    *warn = true;
    switch (patches::status(report)) {
        case patches::Status::FilesOnly:
            return "playguard/tools/patches_note_files"_i18n;
        case patches::Status::SysPatch:
            *warn = false;
            return report.files.empty() ? std::string()
                                        : brls::getStr("playguard/tools/patches_note_redundant", join_list(report.files));
        case patches::Status::SysPatchIncomplete:
            switch (patches::issue(report)) {
                case patches::Issue::NotAtBoot: return "playguard/tools/patches_note_not_at_boot"_i18n;
                case patches::Issue::NoLog:     return "playguard/tools/patches_note_no_log"_i18n;
                case patches::Issue::StaleLog:
                    return brls::getStr("playguard/tools/patches_note_stale",
                                        report.log_fw.empty() ? std::string("?") : report.log_fw,
                                        std::string(report.log_emummc ? "emuMMC" : "sysMMC"));
                case patches::Issue::Skipped:
                    return brls::getStr("playguard/tools/patches_note_skipped",
                                        std::string(info.emummc ? "patch_emummc" : "patch_sysmmc"));
                case patches::Issue::NotPatched:
                    return brls::getStr("playguard/tools/patches_note_unpatched", join_list(patches::unpatched(report)));
                default: break;
            }
            break;
        default: break;
    }
    *warn = false;
    return "";
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
