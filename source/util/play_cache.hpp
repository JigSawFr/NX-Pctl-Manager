// play_cache — the last play data read (core/playstats.h), kept on the SD card
// so the Activity tab and the Overview show it at once on the next run, while
// a new read runs in the background. Pure: the file format and the shift of
// the day windows; play_data does the reading and writing.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/pctl_ops_c.hpp"

namespace play_cache
{

// The file: a small header (magic, version, the struct sizes) then the stats
// with their `count` games only. A file of another build's layout is refused.
std::string encode(const PlayStats& stats);
bool decode(const std::string& bytes, PlayStats& out);

// The data was read `days` local days ago: moves the day windows so that
// today is today (nothing played since, as far as the cache knows). More than
// six days: every window is empty. A negative count (the clock went back)
// makes the windows unusable until the next read.
void shift_days(PlayStats& stats, int days);

}   // namespace play_cache
