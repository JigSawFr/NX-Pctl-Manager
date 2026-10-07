// app — process-wide state: version, the pctl availability probe, and the
// read-only / developer modes.
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
const char* repo_url();

// Read-only mode: the service layer refuses every change (core/write_guard.h)
// and the tabs hide their write controls. On at launch when the firmware is
// newer than the checked one, until the firmware screen says otherwise.
// After a change, call ui::on_mode_changed() to update what is on screen.
bool        read_only();
void        set_read_only(bool on);

// Developer mode: diagnostic tools in the Tools tab and the Play timer tab.
// Saved in the preferences, or on for this session only (firmware screen,
// "read-only + developer mode").
bool        dev_mode();
bool        set_dev_mode(bool on, bool persist);   // false: the preference could not be saved

// False while the app is in the background (HOME menu, system PIN prompt …):
// periodic refreshes skip their pctl queries then, keeping the single
// privileged session free for the system.
bool        in_focus();

}   // namespace app
