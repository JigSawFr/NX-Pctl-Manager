// pt_block_flow — Tools › Developer › "Compare the play-timer block": the
// first time, saves the raw play-timer block as a reference (util/pt_block);
// after that, lists the values that changed since, with buttons to save the
// comparison with the diagnostics or to take the current block as the new
// reference. Only reads the console and writes to the SD card.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace pt_block_flow
{

void open();

}   // namespace pt_block_flow
