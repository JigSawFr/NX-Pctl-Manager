// ui — shared helpers for dialogs, toasts, keyboards and formatting.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "util/patches.hpp"
#include "util/pctl_ops_c.hpp"

namespace ui
{

// Status colours from the PlayGuard palette, one shade per theme (>= 4.5:1),
// and the focus highlight in the icon's teal. register_theme_colors() must
// run before the first lookup (borealis aborts on an unknown theme key).
void     register_theme_colors();
// On the console: the system's Latin / Japanese font as the default font of
// every label, the Chinese one only as a fallback (except for Chinese). Right
// after createWindow(), before the first label exists.
void     use_latin_font();
NVGcolor color_ok();
NVGcolor color_warn();
NVGcolor color_bad();
NVGcolor color_track();     // empty part of the play-time gauge
NVGcolor color_note();      // explanatory notes under a section
NVGcolor color_neutral();   // DetailCell value colour
NVGcolor color_text();      // plain label text colour

// "error 0x00001234" followed by a human explanation when one is known.
// PlayGuard's own results (read-only, invalid value …) are just the sentence:
// there is no code to look up. A system code gets, in a paragraph of its own,
// what to do if it happens again (send a report); `next_step` false leaves it
// out (the start-up error and recovery screens say what to do themselves).
std::string rc_text(Result rc, bool next_step = true);

// Runs `task` on a thread of its own, rather than brls::async: that single
// queue runs one task after the other, so a network request queued there
// waits behind reading the play log and loading game icons (minutes, on a
// console with many games), or behind another request's timeout. Every
// network task goes here; the play log and the icons stay on the queue, one
// at a time on the console's services. Queued there when no thread can be
// had. `what` names it in the log.
void in_background(const char* what, std::function<void()> task);
// At exit, after the main loop: says the app is quitting (quitting() turns
// true, for a task that waits or polls) and waits up to `max` for the tasks
// of in_background still running, so none of them outlives the objects it
// uses (brls::sync's queue, curl, statics). False when some are still running.
bool finish_background(std::chrono::milliseconds max);
bool quitting();

// Deferred to the next frame (toasts fired right after a system applet returns
// were dropped on fw 22.1.0).
void notify(const std::string& text);
// A failure the user asked for something and did not get: a dialog, not a
// toast (a toast is gone before "error 0x…" is read). Next frame, so it can
// be called from a dialog button. Logged as "error: " for the smoke test.
void error(const std::string& text);
// config::save(), with a toast when the SD card refused the write.
bool save_config();
// `next` in place of the current screen. Borealis never pops the first
// activity, so a start screen (lock, rescue) gets `next` pushed over it
// instead: popping it would silently do nothing.
void replace_screen(brls::Activity* next);
// Success: `ok_text` as a toast. Failure: `error_prefix — reason` as a dialog.
void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix);

// Two-button dialog. Cancel (left) has the focus when it opens;
// `confirm_label` (right) runs `on_yes`, Cancel or B runs `on_no`.
// `danger`: the confirm button is drawn in the "bad" colour (see below).
void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no = nullptr, bool danger = false);
// Same with `extra` (a chart, …) under the text; ui takes ownership of it.
void confirm_with(const std::string& body, brls::View* extra, const std::string& confirm_label,
                  std::function<void()> on_yes, std::function<void()> on_no = nullptr, bool danger = false);
// Same, for actions that delete something: the confirm button is drawn in the
// "bad" colour so it never looks like an ordinary OK.
void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes);
void info(const std::string& body);

// Reads the stored PIN (pctl_get_pin) and shows it in large digits, or a
// dialog with the error. The app's copies are wiped as soon as the label
// holds the text. True when it was shown; `on_close` then runs once it is
// closed (OK or B). Used by Security › Show the PIN.
bool show_pin_dialog(std::function<void()> on_close = nullptr);

// A dialog showing `text`. borealis' own text dialog does not scroll: a text
// taller than the screen collapses into one cut line. A long one is laid out
// with a smaller font and narrower margins instead, so it fits.
brls::Dialog* dialog(const std::string& text);

// B closes `dialog` and runs `on_cancel` (borealis' own B only closes it).
void on_cancel(brls::Dialog* dialog, std::function<void()> on_cancel);

// Dropdown list; `on_pick(index)` runs after the list has closed.
void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick);

// System keyboard prompts (callback only on confirm). prompt_minutes accepts
// minutes ("90") or hours:minutes ("1:30"), up to 24:00.
void prompt_minutes(const std::string& header, uint16_t current, std::function<void(uint16_t)> on_value);
void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value);

std::string fmt_minutes(uint16_t minutes);      // a limit: "No limit", "0 min (no play)", "45 min", "2 h 30"
// Seven limits in a few words: "2 h every day", "1 h to 3 h", "1 h to no limit", "No limit".
std::string days_summary(const uint16_t days[7]);
std::string fmt_played(uint16_t minutes);       // time played: "0 min", "45 min", "2 h 30"
std::string fmt_duration_ns(uint64_t ns);       // remaining time, rounded to minutes
std::string fmt_play_time(uint64_t seconds);    // "0 min", "< 1 min", "45 min", "152 h 30" (no 24 h cap)
std::string day_name(int day);                  // 0 = Sunday, as a title ("Monday")
std::string day_name_in_text(int day);          // inside a sentence (fr: "lundi")
std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no);
// "Now" on the console: its user clock read live and its time-zone rule
// (calendar.h says why not time()/localtime()).
LocalTime   local_now();
int         today_weekday();                    // 0 = Sunday
std::string today_date();                       // "2026-10-07"
std::string now_hms();                          // "14:03:12"
std::string now_stamp();                        // "2026-10-07 14:03"

// System / compatibility strings shared by the dashboard and the About section.
std::string fw_text(const SysInfo& info);                 // "23.0.1 · Atmosphère 1.12.0"
std::string compat_text(const SysInfo& info, NVGcolor* color = nullptr);
std::string level_name(uint32_t level);                    // localised restriction level

// Console section (Tools) and Overview warnings.
std::string storage_text(const SysInfo& info);             // "emuMMC (Atmosphère)"
std::string storage_short(const SysInfo& info);            // "emuMMC" / "sysMMC" / "—"
// The other system (sysMMC from emuMMC, or an emuMMC set up on the card from
// sysMMC) has its own parental controls, out of PlayGuard's sight; "" if none.
std::string other_storage_note(const SysInfo& info);
// The serial-number warning only applies on emuMMC: on sysMMC the real serial
// is normal (online play), blanking it would cut Nintendo's services.
bool        serial_warning(const SysInfo& info);
std::string blank_text(const SysInfo& info, NVGcolor* color = nullptr);
// As the system sees it; `reveal` false masks the middle ("XAW1000•••••01").
std::string serial_text(const SysInfo& info, bool reveal);
bool        patches_warning(const patches::Report& report);
// The game-patch report of this boot, read from the SD card once per run (it
// only changes with a reboot): the Overview and Tools no longer rescan it
// each time they are opened.
const patches::Report& patch_report();
std::string patches_text(const patches::Report& report, NVGcolor* color = nullptr);
// Explanation under the Game patches line ("" when there is nothing to say);
// *warn tells whether it is a warning (amber) or a plain note.
std::string patches_note(const patches::Report& report, const SysInfo& info, bool* warn);
std::string time_text(uint64_t posix);                     // local time, or "—" for 0

// Read-only mode: a toast says so and this returns true (the caller then
// does nothing). Else false.
bool refuse_read_only();
// An action that writes stays visible in read-only mode, greyed (title and
// value), so the parent sees what exists; its handler calls
// refuse_read_only(). `title` / `detail`: the colours when writable. A
// BooleanCell gets its own colours back from its state.
void show_writable(brls::DetailCell* cell, bool writable, NVGcolor title, NVGcolor detail);
void show_writable(brls::DetailCell* cell, bool writable);   // plain text, neutral value
// A BooleanCell flips before telling its listener: in read-only mode the
// switch would flip, the write be refused and the switch flip back. This
// checks first, then flips as BooleanCell does.
void guard_switch(brls::BooleanCell* cell);

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
constexpr int activity     = 3;   // play time per game
constexpr int restrictions = 4;
constexpr int clock        = 5;
constexpr int security     = 7;   // PIN, unlock, companion app, delete
constexpr int preferences  = 9;
constexpr int tools        = 10;
constexpr int about        = 11;   // version, credits, changelog
// The n-th tab (0 = Overview) as a sidebar position (separators count).
constexpr int of(int n)
{
    constexpr int positions[] = { dashboard, play_timer, activity, restrictions, clock, security, preferences, tools, about };
    return n >= 0 && n < (int)(sizeof(positions) / sizeof(positions[0])) ? positions[n] : dashboard;
}
}   // namespace tab

// Opens another tab of the main screen and focuses its first item. `from` is
// any view inside the current tab; the switch happens on the next frame
// because the current tab is deleted by it.
void go_to_tab(brls::View* from, int position);

// "Parental controls are temporarily unlocked — Lock now" cell shown at the
// top of the Overview and Play timer tabs (no "Lock now" in read-only mode).
// `after` runs once it relocked.
void init_unlock_banner(brls::DetailCell* cell, std::function<void()> after);
void show_unlock_banner(brls::DetailCell* cell, bool unlocked);

// Asks before quitting so a language / theme change can take effect.
void offer_restart();

// Temporary unlock as last read by any tab (`valid` false: the read failed,
// keep the previous value). The main screen's title says it while unlocked.
void note_unlocked(bool valid, bool unlocked);
bool known_unlocked();

// After app::set_read_only / set_dev_mode: updates the main screen title,
// starts or stops the play-timer recorder (action/pt_log_flow) and re-reads
// the tab on screen.
void on_mode_changed();

}   // namespace ui
