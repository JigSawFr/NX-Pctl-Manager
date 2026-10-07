// app — process-wide state: build flags, version, the pctl availability probe.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace app
{

// Probes the pctl service once (open + immediately release: the privileged
// `pctl:a` slot must stay free for the system). Returns true on success.
bool init();
void shutdown();

bool        pctl_available();
uint32_t    pctl_init_result();   // 0 if the probe succeeded

std::string version();            // "1.0.0"
bool        probe_build();        // built with PROBE=1
bool        read_only_build();    // built with READ_ONLY=1
const char* repo_url();

// False while the app is in the background (HOME menu, system PIN prompt …):
// periodic refreshes skip their pctl queries then, keeping the single
// privileged session free for the system.
bool        in_focus();

}   // namespace app
