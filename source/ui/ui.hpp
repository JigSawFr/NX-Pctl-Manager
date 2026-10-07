// ui — shared helpers for dialogs, toasts, keyboards and formatting.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace ui
{

// Status colours from the PlayGuard palette, one shade per theme (>= 4.5:1).
// register_theme_colors() must run before the first lookup (borealis aborts
// on an unknown theme key).
void     register_theme_colors();
NVGcolor color_ok();
NVGcolor color_warn();
NVGcolor color_bad();
NVGcolor color_track();     // empty part of the play-time gauge
NVGcolor color_neutral();   // DetailCell value colour
NVGcolor color_text();      // plain label text colour

// "error 0x00001234" followed by a human explanation when one is known.
std::string rc_text(Result rc);

// Deferred to the next frame (toasts fired right after a system applet returns
// were dropped on fw 22.1.0).
void notify(const std::string& text);
void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix);

// Two-button dialog. `confirm_label` runs `on_yes`; Cancel runs `on_no`.
void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no = nullptr);
void info(const std::string& body);

// Dropdown list; `on_pick(index)` runs after the list has closed.
void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick);

// System keyboard prompts (callback only on confirm).
void prompt_minutes(const std::string& header, uint16_t current, std::function<void(uint16_t)> on_value);
void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value);

std::string fmt_minutes(uint16_t minutes);      // "No limit", "0 min", "45 min", "2 h 30"
std::string fmt_duration_ns(uint64_t ns);       // remaining time, rounded to minutes
std::string day_name(int day);                  // 0 = Sunday
std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no);
int         today_weekday();                    // 0 = Sunday, from the console's local time

// System / compatibility strings shared by the dashboard and the About section.
std::string fw_text(const SysInfo& info);                 // "23.0.1 · Atmosphère 1.12.0"
std::string compat_text(const SysInfo& info, NVGcolor* color = nullptr);
std::string level_name(uint32_t level);                    // localised restriction level
std::string time_text(uint64_t posix);                     // local time, or "—" for 0

// Hides a view when `visible` is false (Visibility::GONE frees its space).
void set_visible(brls::View* view, bool visible);

}   // namespace ui
