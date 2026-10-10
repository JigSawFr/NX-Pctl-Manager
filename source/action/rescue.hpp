// rescue — the app side of the "last chance" (core/rescue.h): at start-up it
// picks up the report the playguard-rescue sysmodule leaves after it acted on
// a RESCUE file, so PlayGuard can tell the parent what happened and let them
// finish (show / reset the PIN, delete everything). RescueActivity shows it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <optional>

extern "C" {
#include "core/rescue.h"
}

namespace rescue
{

// The report left on the SD card, read and then removed (it is shown once).
// None when there is no report or it is damaged (data_notice then tells the
// parent at start-up). Reading it records a "rescue" entry in the change history.
std::optional<RescueReport> take();

// Whether the console confirms `report` (rescue_report_confirmed, from a
// fresh read of 1206 and 1006), with the sysmodule installed on the SD card
// (a console a parent left temporarily unlocked would otherwise confirm a
// report written by hand). False when the read fails.
bool confirmed(const RescueReport& report);

// Whether a report is waiting (without removing it): decides at start-up
// whether to show the recovery screen instead of the lock screen.
bool pending();

// Whether the sysmodule is on the SD card
// (atmosphere/contents/4200000000505247/exefs.nsp): while it is, anyone who
// can edit the card can unlock or wipe the parental controls at boot.
bool installed();

}   // namespace rescue
