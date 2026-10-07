// ui — shared helpers for dialogs, toasts, keyboards and formatting.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
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
NVGcolor color_note();      // explanatory notes under a section
NVGcolor color_neutral();   // DetailCell value colour
NVGcolor color_text();      // plain label text colour

// "error 0x00001234" followed by a human explanation when one is known.
std::string rc_text(Result rc);

// Deferred to the next frame (toasts fired right after a system applet returns
// were dropped on fw 22.1.0).
void notify(const std::string& text);
void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix);

// Two-button dialog. Cancel (left) has the focus when it opens;
// `confirm_label` (right) runs `on_yes`, Cancel or B runs `on_no`.
void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no = nullptr);
// Same, for actions that delete something: the confirm button is drawn in the
// "bad" colour so it never looks like an ordinary OK.
void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes);
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
std::string day_name(int day);                  // 0 = Sunday, as a title ("Monday")
std::string day_name_in_text(int day);          // inside a sentence (fr: "lundi")
std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no);
int         today_weekday();                    // 0 = Sunday, from the console's local time

// System / compatibility strings shared by the dashboard and the About section.
std::string fw_text(const SysInfo& info);                 // "23.0.1 · Atmosphère 1.12.0"
std::string compat_text(const SysInfo& info, NVGcolor* color = nullptr);
std::string level_name(uint32_t level);                    // localised restriction level
std::string time_text(uint64_t posix);                     // local time, or "—" for 0

// Hides a view when `visible` is false (Visibility::GONE frees its space).
// When the hidden view had the focus, the focus moves to the nearest visible
// focusable sibling (the next one on a tie), so it never lands on the list
// container or jumps far away.
void set_visible(brls::View* view, bool visible);

// Applies several visibility changes, showing views before hiding others, so a
// cell that replaces the focused one (Unlock -> Lock now) is there to take
// the focus.
void set_visible_all(std::initializer_list<std::pair<brls::View*, bool>> changes);

// Sidebar positions of the main screen tabs, in the order of
// resources/xml/activity/main.xml (separators count as positions).
namespace tab
{
constexpr int dashboard    = 0;
constexpr int play_timer   = 2;
constexpr int restrictions = 3;
constexpr int clock        = 4;
constexpr int pairing      = 6;
constexpr int security     = 7;
constexpr int tools        = 9;
}   // namespace tab

// Opens another tab of the main screen and focuses its first item. `from` is
// any view inside the current tab; the switch happens on the next frame
// because the current tab is deleted by it.
void go_to_tab(brls::View* from, int position);

// "Parental controls are temporarily unlocked — Lock now" cell shown at the
// top of the Overview and Play timer tabs. `after` runs once it relocked.
void init_unlock_banner(brls::DetailCell* cell, std::function<void()> after);
void show_unlock_banner(brls::DetailCell* cell, bool unlocked);

// Asks before quitting so a language / theme change can take effect.
void offer_restart();

}   // namespace ui
