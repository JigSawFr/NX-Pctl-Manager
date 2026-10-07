// duration — reads a daily limit typed as minutes ("90") or hours and minutes
// ("1:30", "1h30", "2h"). Plain C++ so the host tests can run it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace duration
{

// False when the text is not a duration or exceeds 24 h (1440 min).
bool parse(const std::string& text, uint16_t* minutes);

// "1:30", "0:45", "24:00": the form the keyboard is pre-filled with.
std::string format_hm(uint16_t minutes);

}   // namespace duration
