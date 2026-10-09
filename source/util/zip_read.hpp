// zip_read — takes one file out of a zip archive (a GitHub Actions artifact):
// stored or deflated, checked against the CRC-32 and size the archive
// records. No Zip64 (an artifact here is a few MB). Plain C++ on zlib for
// the host tests (tests/dev_builds).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace zip_read
{

// Writes the entry `name` of the archive `zip_path` to `out_path`. False,
// and *error set (in English), when it is missing, larger than `max_size`,
// damaged or does not match its CRC-32; `out_path` is then removed.
bool extract(const std::string& zip_path, const std::string& name, const std::string& out_path, uint64_t max_size,
             std::string* error);

}   // namespace zip_read
