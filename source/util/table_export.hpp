// table_export — writes a small table (the Activity tab's figures) to a file
// as CSV, JSON, XLSX or PDF, with nothing beyond nlohmann/json: the XLSX is an
// uncompressed zip of the minimal SpreadsheetML parts, the PDF a plain text
// layout in the standard Helvetica font (Latin-1 text; other characters print
// as "?"). Plain C++ (no UI, no libnx) so the host tests can run it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace table_export
{

enum class Format { Csv, Json, Xlsx, Pdf };

struct Column
{
    std::string key;        // JSON field name, e.g. "today_min"
    std::string label;      // header in CSV / XLSX / PDF (localised)
    bool numeric = false;   // a whole number: unquoted, a JSON number, a numeric cell
};

struct Table
{
    std::string title;      // first line of the XLSX / PDF, and in the JSON
    std::string subtitle;   // e.g. the export date
    std::string sheet;      // XLSX sheet name
    std::vector<Column> columns;
    // One string per column. A numeric cell is a decimal integer, or empty
    // for "no value" (JSON null, an empty cell).
    std::vector<std::vector<std::string>> rows;
};

const char* extension(Format f);   // "csv", "json", "xlsx", "pdf"

// The file content (binary for XLSX and PDF).
std::string render(const Table& t, Format f);

// Writes <dir>/<base>_<YYYYMMDD_HHMMSS>.<ext> (local time). Returns the path,
// or "" and a short English reason in *error.
std::string save(const Table& t, Format f, const std::string& dir, const std::string& base,
                 std::string* error = nullptr);

}   // namespace table_export
