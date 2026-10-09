// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "action/history_flow.hpp"
#include "activity/main_activity.hpp"
#include "app.hpp"
#include "core/platform.h"
#include "tab/tab_base.hpp"
#include "util/config.hpp"
#include "util/duration.hpp"
#include "util/paths.hpp"

#include <algorithm>
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
    // The focus highlight, the click pulse and the active sidebar item in the
    // icon's teal (borealis' defaults are the Switch's cyan / blue), so the
    // app looks like its icon. Values keep borealis' blue: teal values would
    // read as the "ok" state.
    light.addColor("brls/highlight/color1", nvgRGB(0x14, 0xA3, 0x8A));
    light.addColor("brls/highlight/color2", nvgRGB(0x4A, 0xD6, 0xBA));
    light.addColor("brls/click_pulse", nvgRGBA(0x14, 0xA3, 0x8A, 38));
    light.addColor("brls/sidebar/active_item", nvgRGB(0x0F, 0x8A, 0x74));
    light.addColor("brand/ok", nvgRGB(0x0A, 0x6E, 0x5C));
    light.addColor("brand/warn", nvgRGB(0x8A, 0x52, 0x00));
    light.addColor("brand/bad", nvgRGB(0xB7, 0x1C, 0x1C));
    light.addColor("brand/gauge_track", nvgRGBA(0, 0, 0, 34));
    light.addColor("brand/note", nvgRGB(0x5C, 0x5C, 0x5C));
    auto& dark = brls::Theme::getDarkTheme();
    dark.addColor("brls/highlight/color1", nvgRGB(0x2E, 0xC4, 0xA6));
    dark.addColor("brls/highlight/color2", nvgRGB(0x9A, 0xF0, 0xDC));
    dark.addColor("brls/click_pulse", nvgRGBA(0x2E, 0xC4, 0xA6, 38));
    dark.addColor("brls/sidebar/active_item", nvgRGB(0x2E, 0xC4, 0xA6));
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
            case 9: return "playguard/error/not_confirmed"_i18n;
            case 10: return "playguard/error/no_pin"_i18n;
            case 11: return "playguard/error/relock_failed"_i18n;
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
    if (NXM_IS_APP_RESULT(rc) && !hint.empty()) {
        if (hint[0] >= 'a' && hint[0] <= 'z') hint[0] = (char)(hint[0] - 'a' + 'A');
        return hint;
    }
    std::string code = fmt::format("0x{:08X}", (unsigned)rc);
    return hint.empty() ? brls::getStr("playguard/error/code", code)
                        : brls::getStr("playguard/error/code_hint", code, hint);
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

void on_cancel(brls::Dialog* dialog, std::function<void()> on_cancel)
{
    dialog->setCancelable(true);
    dialog->getAppletFrame()->registerAction(
        "hints/back"_i18n, brls::BUTTON_B,
        [dialog, on_cancel](brls::View*) {
            dialog->close([on_cancel]() { if (on_cancel) on_cancel(); });
            return true;
        },
        false, false, brls::SOUND_BACK);
}

// Lines `text` takes at `per_line` characters a line (a rough count: the
// font is proportional, but the paragraphs are what makes a text tall).
static int estimated_lines(const std::string& text, size_t per_line)
{
    int lines = 0;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        // UTF-8 continuation bytes are not characters.
        size_t chars = 0;
        for (size_t i = start; i < end; i++) chars += ((unsigned char)text[i] & 0xC0) != 0x80;
        lines += chars == 0 ? 1 : (int)((chars + per_line - 1) / per_line);
        start = end + 1;
    }
    return lines;
}

brls::Dialog* dialog(const std::string& text)
{
    // borealis' layout (720 px wide, 115 px side margins, 24 px font) holds
    // about 40 characters a line and 13 lines above the buttons.
    if (estimated_lines(text, 40) <= 13) return new brls::Dialog(text);
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(19);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    auto* box = new brls::Box();
    box->addView(label);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(28, 40, 28, 40);
    return new brls::Dialog(box);
}

// The text (slightly smaller, so a chart fits under a long one) and `extra`.
static brls::Dialog* dialog_with(const std::string& text, brls::View* extra)
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(20);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->setPadding(26, 40, 18, 40);
    box->addView(label);
    extra->setMarginTop(18);
    box->addView(extra);
    return new brls::Dialog(box);
}

static void open_confirm(const std::string& body, const std::string& confirm_label,
                         std::function<void()> on_yes, std::function<void()> on_no, bool danger,
                         brls::View* extra = nullptr)
{
    auto* dialog = extra ? dialog_with(body, extra) : ui::dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(confirm_label, [on_yes]() { if (on_yes) on_yes(); });
    on_cancel(dialog, on_no);
    if (danger)
        if (auto* button = dynamic_cast<brls::Button*>(dialog->getView("brls/dialog/button2")))
            button->setTextColor(color_bad());
    dialog->open();
}

void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), danger);
}

void confirm_with(const std::string& body, brls::View* extra, const std::string& confirm_label,
                  std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), danger, extra);
}

void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes)
{
    open_confirm(body, confirm_label, std::move(on_yes), nullptr, true);
}

void info(const std::string& body)
{
    auto* dialog = ui::dialog(body);
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
    // The system number pad, with a ":" key (core/platform.h); borealis' own
    // text input where there is none.
    char out[16] = {};
    switch (platform_numpad(header.c_str(), guide.c_str(), initial.c_str(), 5, out, sizeof(out))) {
        case PLATFORM_INPUT_OK:        handle(out); return;
        case PLATFORM_INPUT_CANCELLED: return;
        case PLATFORM_INPUT_NONE:      break;
    }
    brls::Application::getImeManager()->openForText([handle](std::string text) { handle(text); },
                                                   header, guide, 5, initial);
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

std::string days_summary(const uint16_t days[7])
{
    bool uniform = true, any_nolimit = false;
    int lo = -1, hi = -1;
    for (int d = 0; d < 7; d++) {
        uniform &= days[d] == days[0];
        if (days[d] == PT_DAY_NOLIMIT) {
            any_nolimit = true;
            continue;
        }
        lo = lo < 0 ? days[d] : std::min<int>(lo, days[d]);
        hi = hi < 0 ? days[d] : std::max<int>(hi, days[d]);
    }
    if (uniform)
        return days[0] == PT_DAY_NOLIMIT ? "playguard/common/no_limit"_i18n
                                         : brls::getStr("playguard/play_timer/state/every_day", fmt_minutes(days[0]));
    if (lo < 0) return "playguard/common/no_limit"_i18n;
    // Inside the sentence: "1 h to no limit", not "1 h to No limit".
    const std::string top = any_nolimit ? "playguard/common/no_limit_in_text"_i18n : fmt_minutes((uint16_t)hi);
    return brls::getStr("playguard/play_timer/profile_range", fmt_minutes((uint16_t)lo), top);
}

std::string fmt_played(uint16_t m)
{
    if (m == 0) return brls::getStr("playguard/common/minutes", 0);
    return fmt_minutes(m);
}

std::string fmt_duration_ns(uint64_t ns)
{
    uint64_t minutes = (ns + 30000000000ULL) / 60000000000ULL;
    if (minutes > 1440) minutes = 1440;
    return fmt_played((uint16_t)minutes);   // a time left: "0 min", not "0 min (no play)"
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

LocalTime local_now()
{
    LocalTime l{};
    u64 posix = 0;
    if (time_local_now(&posix, &l)) return l;
    // No time-zone rule: the C library's idea of local time.
    std::time_t now = (std::time_t)posix;
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    l.year = (uint16_t)(tmv.tm_year + 1900);
    l.month = (uint8_t)(tmv.tm_mon + 1);
    l.day = (uint8_t)tmv.tm_mday;
    l.hour = (uint8_t)tmv.tm_hour;
    l.minute = (uint8_t)tmv.tm_min;
    l.second = (uint8_t)tmv.tm_sec;
    l.wday = (uint8_t)tmv.tm_wday;
    return l;
}

int today_weekday()
{
    return local_now().wday;
}

std::string today_date()
{
    const LocalTime l = local_now();
    return fmt::format("{:04d}-{:02d}-{:02d}", (int)l.year, (int)l.month, (int)l.day);
}

std::string now_hms()
{
    const LocalTime l = local_now();
    return fmt::format("{:02d}:{:02d}:{:02d}", (int)l.hour, (int)l.minute, (int)l.second);
}

std::string now_stamp()
{
    const LocalTime l = local_now();
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}", (int)l.year, (int)l.month, (int)l.day, (int)l.hour, (int)l.minute);
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

const patches::Report& patch_report()
{
    static bool read = false;
    static patches::Report report;
    if (!read) {
        SysInfo si;
        sysinfo_get(&si);
        char fw[16];
        sysinfo_version_string(si.hos_version, fw, sizeof(fw));
        report = patches::detect(paths::sd_root(), fw, si.emummc);
        read = true;
    }
    return report;
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

bool refuse_read_only()
{
    if (!app::read_only()) return false;
    notify(rc_text(NXM_RC_READ_ONLY));
    return true;
}

void show_writable(brls::DetailCell* cell, bool writable, NVGcolor title, NVGcolor detail)
{
    const NVGcolor grey = nvgTransRGBA(color_text(), 110);
    cell->title->setTextColor(writable ? title : grey);
    if (auto* sw = dynamic_cast<brls::BooleanCell*>(cell)) {
        if (writable) sw->setOn(sw->isOn(), false);   // its own value colours back
        else sw->detail->setTextColor(grey);
        return;
    }
    cell->detail->setTextColor(writable ? detail : grey);
}

void show_writable(brls::DetailCell* cell, bool writable)
{
    show_writable(cell, writable, color_text(), color_neutral());
}

void guard_switch(brls::BooleanCell* cell)
{
    cell->registerClickAction([cell](brls::View*) {
        if (refuse_read_only()) return true;
        cell->setOn(!cell->isOn());
        cell->getEvent()->fire(cell->isOn());
        return true;
    });
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
    cell->setDetailText("playguard/security/banner_action"_i18n);
    cell->registerClickAction([after](brls::View*) {
        if (app::read_only()) {
            notify(rc_text(NXM_RC_READ_ONLY));
            return true;
        }
        Result rc = pctl_relock();
        if (R_SUCCEEDED(rc)) history_flow::record_event("relock");
        notify_result(rc, "playguard/toast/relocked"_i18n, "playguard/toast/relock_err"_i18n);
        if (after) after();
        return true;
    });
}

void show_unlock_banner(brls::DetailCell* cell, bool unlocked)
{
    // Read-only: no action, so the title gets the whole width (an empty detail
    // label still reserves its space).
    cell->detail->setVisibility(app::read_only() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
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

namespace
{
void wipe(char* p, size_t n)
{
    volatile char* b = p;
    while (n--) *b++ = 0;
}
}   // namespace

void show_pin_dialog()
{
    char pin[16];
    Result rc = pctl_get_pin(pin, sizeof(pin));
    brls::Logger::info("pctl_get_pin returned 0x{:08X}", (unsigned)rc);
    if (R_FAILED(rc)) {
        notify_result(rc, "", "playguard/security/show_pin_err"_i18n);
        return;
    }
    std::string spaced;   // "1 2 3 4": easier to read out and to type
    spaced.reserve(2 * sizeof(pin));   // no reallocation, so no stray copy
    for (const char* c = pin; *c; c++) {
        if (!spaced.empty()) spaced += ' ';
        spaced += *c;
    }
    wipe(pin, sizeof(pin));

    auto* title = new brls::Label();
    title->setText("playguard/security/show_pin_title"_i18n);
    title->setFontSize(22);
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    title->setTextColor(color_note());
    auto* digits = new brls::Label();
    digits->setText(spaced);
    digits->setFontSize(56);
    digits->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    digits->setMarginTop(16);
    wipe(&spaced[0], spaced.size());

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(40, 40, 40, 40);
    box->addView(title);
    box->addView(digits);

    auto* d = new brls::Dialog(box);
    d->addButton("hints/ok"_i18n, []() {});
    d->setCancelable(true);
    d->open();
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
    for (brls::Activity* activity : brls::Application::getActivitiesStack())
        if (auto* main = dynamic_cast<MainActivity*>(activity)) main->update_title();
    TabBase::refresh_shown();
}

}   // namespace ui
