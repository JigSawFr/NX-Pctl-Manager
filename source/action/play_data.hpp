// play_data — the console's play data (core/playstats.h) read off the main
// thread and kept for the whole app: the Activity tab, its account filter and
// the Overview's "played today" share it, so the log is walked once per
// account and per few minutes rather than once per screen. The last read of
// each is also kept on the SD card (data_dir()/cache/, util/play_cache.hpp),
// so the next run shows it at once while it reads the log again. UI thread
// only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace play_data
{

// Which data: every account ("") or one ("<uid hex>"), see key_of().
using Key = std::string;
Key key_of(const PlayAccount* account);   // nullptr: every account

// The last read for `key` (the last run's, from the SD card, before this
// run's first), or null when there is none.
std::shared_ptr<const PlayStats> latest(const Key& key = "");
// The last read for `key` was made by this run and is at most `max_age` old.
bool fresh(const Key& key, std::chrono::seconds max_age);
// A read for `key` is queued or running.
bool busy(const Key& key);

// Reads `account`'s data (nullptr: every account) unless a read for it already
// runs. Every listener is told when any read ends.
void fetch(const PlayAccount* account);

// Called on the UI thread after each read; returns an id for unlisten().
int  listen(std::function<void()> on_read);
void unlisten(int id);

// The console's user accounts, read once per run (acc:u0 is quick).
const std::vector<PlayAccount>& accounts();

// Seconds played today, every game added up.
uint64_t today_total_s(const PlayStats& stats);

}   // namespace play_data
