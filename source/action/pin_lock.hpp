// pin_lock — Security › "Ask for the PIN": the parental-control PIN (in the
// system's own PIN screen) before PlayGuard changes anything, or before it
// opens at all.
//
//   off      never (the default);
//   changes  before the first change or "Show the PIN", then not again for
//            5 minutes: anyone can look, only the parent can change;
//   open     to open PlayGuard (a lock screen until it is entered).
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

// At start-up, once: installs the check in the service layer.
void install();

// The setting is "open": start on the lock screen.
bool at_start();

// Asks for the PIN now. True when it was entered, or when no PIN is set.
bool ask();

// Around an order of the remote link applied with its "auto" policy (UI
// thread): the check lets the changes through. The broker authenticated the
// sender; the PIN prompt is for someone at the console.
void remote_bypass(bool on);

// The check a change goes through, for a setting that is not on the console
// but lets one be made (the remote link's settings): true when no PIN is
// needed, it was entered (or recently), false when refused.
bool allow_change();

// "Never" / "Before a change" / "To open PlayGuard".
std::string mode_text();
// One sentence on what the chosen mode means (shown under the setting).
std::string note_text();

// Security's picker. Turning it on needs a PIN on the console; turning it
// down asks for the PIN first. `done` runs after a change.
void choose(std::function<void()> done);

}   // namespace pin_lock
