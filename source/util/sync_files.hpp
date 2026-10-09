// sync_files — the remote link's files on the SD card, from PlayGuard's side:
// sync.conf (the link's settings, written by the Sync screen), and what the
// agent sysmodule reads in sync/ to act on PlayGuard's behalf while it is
// closed: nro_state.txt (the records of config.json it needs), profiles.txt
// (the saved profiles, for profile orders) and names.txt (game names, for
// "now playing"). One writer per file, flat "key=value" text the agent's C
// parser reads (source/sync/sync_conf.h). No UI and no console calls:
// host-tested (tests/sync_files).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "sync/sync_conf.h"
#include "sync/sync_exec.h"
#include "util/config.hpp"
#include "util/profiles.hpp"

namespace sync_files
{

std::string conf_file();   // data_dir()/sync.conf
std::string dir();         // data_dir()/sync
bool exists();             // sync.conf is there: the link was set up once

// The defaults, then what sync.conf holds.
SyncConf load();
bool save(const SyncConf& c, std::string* error = nullptr);
// Gives the console its id ("a1b2c3d4", from 4 random bytes) when it has
// none. True when it changed.
bool ensure_id(SyncConf& c, const uint8_t random[4]);

// config.json's records <-> the shared form sync_exec updates.
SyncRecords records_from(const config::Config& c);
// True when something changed.
bool records_into(const SyncRecords& r, config::Config& c);

// sync/nro_state.txt: the records, extra_auto_restore, the firmware choice
// and the PIN lock. Written after every config save while sync.conf exists
// (config::set_saved_hook).
std::string nro_state_text(const config::Config& c);
void export_nro_state();

// sync/profiles.txt: one "60,90,120,120,120,180,180=School week" line per
// profile (minutes Sunday first, 65535 no limit).
std::string profiles_text(const std::vector<profiles::Profile>& list);
bool write_profiles(const std::vector<profiles::Profile>& list);

// sync/names.txt: one "0100000000010000=Super Mario Odyssey" line per game.
std::string names_text(const std::vector<std::pair<uint64_t, std::string>>& names);
bool write_names(const std::vector<std::pair<uint64_t, std::string>>& names);

}   // namespace sync_files
