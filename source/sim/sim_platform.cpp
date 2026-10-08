// Desktop simulation of core/platform.h: PLAYGUARD_SIM_* variables answer
// for the console (listed in platform.h); what would reach the console is
// printed instead, so the smoke test can read it from the log.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>

PlatformInput platform_numpad(const char* header, const char*, const char*, int max_len, char* out, size_t out_size)
{
    const char* typed = std::getenv("PLAYGUARD_SIM_NUMPAD");
    if (!typed) return PLATFORM_INPUT_NONE;   // borealis' own text input
    if (!out || out_size == 0 || std::strcmp(typed, "cancel") == 0) return PLATFORM_INPUT_CANCELLED;
    size_t n = std::strlen(typed);
    if (max_len >= 0 && n > (size_t)max_len) n = (size_t)max_len;
    if (n >= out_size) n = out_size - 1;
    std::memcpy(out, typed, n);
    out[n] = '\0';
    std::printf("numpad: %s -> %s\n", header ? header : "", out);
    std::fflush(stdout);
    return out[0] ? PLATFORM_INPUT_OK : PLATFORM_INPUT_CANCELLED;
}

bool platform_in_focus(void)
{
    return std::getenv("PLAYGUARD_SIM_BACKGROUND") == nullptr;
}

bool platform_can_launch(void)
{
    return std::getenv("PLAYGUARD_SIM_HBLOADER") != nullptr;
}

bool platform_set_next_load(const char* path)
{
    if (!path || !platform_can_launch()) return false;
    std::printf("next load: %s\n", path);
    std::fflush(stdout);
    return true;
}

bool platform_region(int* region)
{
    const char* value = std::getenv("PLAYGUARD_SIM_REGION");
    if (!region || !value || !*value) return false;
    char* end = nullptr;
    const long r = std::strtol(value, &end, 10);
    if (*end || r < 0 || r > 255) return false;
    *region = (int)r;
    return true;
}

bool platform_random(void* buf, size_t size)
{
    if (!buf) return false;
    try {
        std::random_device random;
        auto* bytes = static_cast<unsigned char*>(buf);
        for (size_t i = 0; i < size; i++) bytes[i] = static_cast<unsigned char>(random());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
