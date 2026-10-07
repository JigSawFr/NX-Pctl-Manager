// update_flow — "Check for updates" and handing over to a homebrew store
// (Tools › Update with: sphaira, Homebrew App Store, or by hand).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/update.hpp"

namespace update_flow
{

// Checks in the background, then says what it found and offers to update.
void check_now();

// Says which store opens, then closes PlayGuard and starts it; explains how
// to update by hand when no store can be opened.
void open_store();

// "PlayGuard 1.1.0 supports firmware 24.0.0 …" for the firmware screen.
std::string status_text(const update::Result& result, const std::string& firmware);

// "Update with sphaira" / "Update…" (label of the update actions).
std::string update_label();

}   // namespace update_flow
