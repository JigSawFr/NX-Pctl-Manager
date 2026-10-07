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
std::string config_file();

// mkdir -p. Returns false (errno preserved) on failure.
bool ensure_dir(const std::string& dir);

// Writes `content` to `path` through a ".tmp" sibling + rename, so a crash or a
// full SD card never leaves a half-written file behind. On failure returns
// false and puts a short English reason in *error (when non-null).
bool atomic_write(const std::string& path, const std::string& content, std::string* error = nullptr);

bool read_file(const std::string& path, std::string& out);

// Names (not paths) of regular files in `dir` ending with `suffix`, sorted.
std::vector<std::string> list_files(const std::string& dir, const std::string& suffix);

}   // namespace paths
