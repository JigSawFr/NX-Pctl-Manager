// pin_lock — Security › "Ask for the PIN": the parental-control PIN (in the
// system's own PIN screen) before PlayGuard changes anything, or before it
// opens at all.
//
//   off      never, except before "Show the PIN" (the PIN opens everything);
//   changes  before the first change, then not again for 5 minutes: anyone
//            can look, only the parent can change (the default, also when
//            config.json is missing, damaged or holds an unknown value);
//   open     to open PlayGuard (a lock screen until it is entered), and again
//            after 5 minutes or more out of focus.
//
// "Show the PIN" asks every time, whatever the mode (the reveal check,
// write_guard.h).
//
// The check sits in the service layer (write_guard.h), so no change can skip
// it; locking again never asks. Without a PIN on the console nothing is asked
// (and the setting cannot be turned on). The setting itself is on the SD
// card: it keeps a child out of PlayGuard, not someone who edits the card.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

namespace pin_lock
{

// At start-up, once: installs the checks in the service layer.
void install();

// At start-up, once: runs `lock` (on the UI thread) when PlayGuard comes back
// into focus and "open" wants the lock screen again (pin_lock_logic::lock_again).
void watch_focus(std::function<void()> lock);

// The setting is "open": start on the lock screen.
bool at_start();

// Asks for the PIN now. True when it was entered, or when no PIN is set.
bool ask();
// Before showing the PIN: asks for it now, in every mode, ignoring the grace
// and the lock screen. True when it may be shown (the read right after does
// not ask again). On a refusal it says why.
bool before_show_pin();
// What to tell the user after ask() said no: "the PIN was not entered", with
// the result the PIN screen gave (its code tells a cancel from a refusal).
std::string refusal_text();

// "Never" / "Before a change" / "To open PlayGuard".
std::string mode_text();
// One sentence on what the chosen mode means (shown under the setting).
std::string note_text();

// Security's picker. Turning it on needs a PIN on the console; turning it
// down asks for the PIN first. `done` runs after a change.
void choose(std::function<void()> done);

}   // namespace pin_lock
