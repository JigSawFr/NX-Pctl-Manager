// paths — where the app keeps its files, plus small filesystem helpers.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace paths
{

// sd:/switch/playguard on the console; ./playguard_data on desktop.
std::string data_dir();
std::string logs_dir();
std::string profiles_dir();
std::string backups_dir();
std::string exports_dir();
std::string config_file();
std::string history_file();   // the change history (util/history.hpp)

// Root of the SD card: "/" on the console; ./playguard_data/sd on desktop,
// so the simulated build can be given fake Atmosphère / sys-patch files.
std::string sd_root();

// mkdir -p. Returns false (errno preserved) on failure.
bool ensure_dir(const std::string& dir);

// Writes `content` to `path` through a ".tmp" sibling + rename, so a crash or a
// full SD card never leaves a half-written file behind. On failure returns
// false and puts a short English reason in *error (when non-null).
bool atomic_write(const std::string& path, const std::string& content, std::string* error = nullptr);

// Whole file. When `path` is missing but `path`.tmp exists (atomic_write
// stopped between its remove and its rename), reads that one instead.
bool read_file(const std::string& path, std::string& out);

// Whether `path` or its ".tmp" (see read_file) is there at all: one that is
// there but cannot be read is damaged, which is not the same as none.
bool exists(const std::string& path);

// Moves a damaged `path` (or its ".tmp", when only that one is left) to
// `path`.bad, so no later write replaces it. A `path`.bad already there
// becomes `path`.bad.1 (an older .bad.1 is dropped): the two most recent
// damaged copies are kept. False when it could not be moved.
bool put_aside(const std::string& path);

// Names (not paths) of regular files in `dir` ending with `suffix`, sorted.
std::vector<std::string> list_files(const std::string& dir, const std::string& suffix);

}   // namespace paths
