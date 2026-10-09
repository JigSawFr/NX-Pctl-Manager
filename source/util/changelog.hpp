// CHANGELOG.md (bundled in the romfs) as lines the About tab can show: the
// release headings, their sections and items, as plain text.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace changelog
{

struct Line
{
    enum Kind
    {
        Release,   // "## [1.1.0](…) (2026-10-10)" → "1.1.0 · 2026-10-10"
        Section,   // "### 🚀 Features" → "Features"
        Item,      // "* **ui:** text ([#12](…)) ([abc1234](…))" → "ui: text", indented by `depth`
        Text,      // any other paragraph line
    };
    Kind        kind;
    std::string text;
    int         depth = 0;   // Item only: 0 for a top-level bullet
};

// The lines of `md`, at most `max_releases` "## " headings and what follows
// them (0: all). Links keep their text, commit and pull-request references
// are dropped, and so are the emoji (the console's font has none) and the
// "**" / "`" marks. The "# Changelog" title is left out.
std::vector<Line> parse(const std::string& md, int max_releases);

}   // namespace changelog
