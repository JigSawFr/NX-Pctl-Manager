// support — what the main screen shows once at start-up, when nothing else
// does: "What's new" once after each update, else the monthly "Support
// PlayGuard" reminder (unless turned off in Preferences). Plain C++ so the
// host tests can run it; action/support_flow.cpp shows the screens.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace support
{

constexpr int REMINDER_DAYS = 30;

enum class Show
{
    Nothing,
    WhatsNew,   // the running version's notes, once after an update
    Reminder,   // "Support PlayGuard", once a month
};

struct State
{
    bool        reminder_on = true;   // Preferences › monthly reminder
    std::string reminded;             // "YYYY-MM-DD": last reminder, or when counting started
    std::string seen_version;         // the version whose "What's new" was handled
};

struct Decision
{
    Show  show = Show::Nothing;
    State next;   // to save when it differs from the state given
};

// Days from date `a` to date `b` ("YYYY-MM-DD"); false if either is malformed.
bool days_between(const std::string& a, const std::string& b, int* days);

// At start-up, on the main screen with nothing in front of it.
// - First time (no seen_version): remembers this version and starts the
//   month, shows nothing: a new install has the first steps, not news.
// - Another version than seen_version: "What's new" when it has notes; the
//   month starts again either way (no reminder right after an update).
// - Else the reminder, when on and REMINDER_DAYS have passed. A clock set
//   back (a date before `reminded`) starts the month again.
Decision decide(const State& state, const std::string& version, const std::string& today, bool has_notes);

}   // namespace support
