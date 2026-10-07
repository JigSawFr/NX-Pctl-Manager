// clock_check — Tools › "Check the network clock at start-up": one NTP sample
// in the background at start-up, a toast when the network clock (the one the
// play timer counts on) is more than a minute off. It never sets anything:
// setting the clock stays in the Network clock tab.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace clock_check
{

void at_start();

}   // namespace clock_check
