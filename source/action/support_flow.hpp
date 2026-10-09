// support_flow — at start-up, once the main screen is up with nothing in
// front of it (firmware screen, first steps, relock questions …): "What's
// new" once after an update, else the monthly "Support PlayGuard" reminder,
// unless it was turned off (Preferences › At start-up). util/support.hpp
// decides; this shows.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

namespace support_flow
{

void at_start(brls::Activity* main);

}   // namespace support_flow
