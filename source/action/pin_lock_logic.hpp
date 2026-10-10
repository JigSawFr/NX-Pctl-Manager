// pin_lock_logic — the decisions behind pin_lock, without any UI or clock:
// whether a change goes ahead, is refused or asks for the PIN, and what
// moving between the modes needs. Times are passed in so the host tests can
// run it (tests/pin_lock_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <string>

namespace pin_lock_logic
{

using Clock = std::chrono::steady_clock;

// After the PIN was entered, changes go ahead without asking again.
constexpr auto GRACE = std::chrono::minutes(5);
// One action can make several changes (a restore): after a refusal, the
// next ones are refused too instead of asking once per change.
constexpr auto REFUSAL_HOLDS = std::chrono::seconds(3);

// The modes, in order of protection (config pin_lock).
extern const char* const MODES[3];

// A mode's place in MODES; an unknown one counts as "off".
int rank(const std::string& mode);

enum class Check
{
    Allow,    // not "changes", or the PIN was entered less than GRACE ago
    Refuse,   // refused less than REFUSAL_HOLDS ago
    Ask,      // ask for the PIN now
};
// The change check at `now`, given until when the last entry and the last
// refusal hold.
Check check(const std::string& mode, Clock::time_point now, Clock::time_point confirmed_until,
            Clock::time_point refused_until);

enum class Change
{
    NoPin,     // turning it on without a PIN on the console: it would protect nothing
    AskPin,    // less protection than now: only the parent may choose it
    Allowed,
};
// Moving from mode `from` to MODES[to] (a different one).
Change change(const std::string& from, int to, bool has_pin);

}   // namespace pin_lock_logic
