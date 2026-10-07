// fw_gate — a firmware newer than the checked range (PCTL_FW_TESTED_MAX in
// core/sysinfo.h). PlayGuard starts read-only there; FirmwareGateActivity
// looks for a release that supports the firmware and lets the user choose how
// to continue. The choice can be remembered for this firmware with this app
// version (a newer app version asks again, unless it supports the firmware).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace fw_gate
{

enum class Choice
{
    ReadOnly,   // nothing can be changed
    Probe,      // read-only + developer mode, to investigate the firmware
    Risk,       // everything enabled, at the user's own risk
};

bool        needed();       // the firmware is newer than the checked range
std::string firmware();     // "24.0.0"
std::string tested_max();   // "23.0.1"

// Before the main screen is built: read-only when needed(), then the
// remembered choice if it matches this firmware and app version.
void prepare();

// Once the main screen is up: opens the firmware screen, or, when a choice was
// remembered, checks for an update in the background and says when one
// supports this firmware.
void on_main_screen();

// Applies a choice for this session (and saves it when `remember`). The
// caller then calls ui::on_mode_changed().
void apply(Choice choice, bool remember);
void forget();             // drops the remembered choice

// For the diagnostic report: "none", "read_only", "probe (remembered)" …
std::string summary();

}   // namespace fw_gate
