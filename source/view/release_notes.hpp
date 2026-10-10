// release_notes — the running version's notes from the bundled CHANGELOG.md, as views: in
// About and on the "What's new" screen shown once after an update.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

#include "util/changelog.hpp"

namespace release_notes
{

// This version's lines (empty: none, e.g. a development build); `date`
// receives the release date when there is one.
std::vector<changelog::Line> current(std::string* date = nullptr);

// Adds `lines` to `box`: section titles, then their items with the bullet in
// its own column (wrapped lines stay under the text).
void fill(brls::Box* box, const std::vector<changelog::Line>& lines);

}   // namespace release_notes
