// Host tests for source/util/changelog.cpp: CHANGELOG.md as release-please
// writes it, read into the lines the About tab shows.
#include "check.h"
#include <cstdio>
#include <string>

#include "util/changelog.hpp"

using changelog::Line;

static const char* MD =
    "# Changelog\n"
    "\n"
    "## [1.1.0](https://github.com/jigsawfr/playguard/compare/v1.0.0...v1.1.0) (2026-10-10)\n"
    "\n"
    "\n"
    "### \xF0\x9F\x9A\x80 Features\n"   // 🚀
    "\n"
    "* **ui:** an About tab with the changelog ([#33](https://github.com/x/y/issues/33)) ([0123abc](https://github.com/x/y/commit/0123abcdef))\n"
    "* a long item\n"
    "  that goes on\n"
    "  * a nested one\n"
    "\n"
    "### \xE2\x9A\xA0\xEF\xB8\x8F Changes\n"   // ⚠️
    "\n"
    "- uses `romfs` and [borealis](https://github.com/xfangfang/borealis)\n"
    "\r\n"
    "## 1.0.0 (2026-09-01)\n"
    "\n"
    "First release.\n"
    "\n"
    "## Upstream history\n"
    "\n"
    "### Pctl Manager v3.0.0\n";

int main()
{
    const auto all = changelog::parse(MD, 0);
    CHECK(all.size() == 11);
    CHECK(all[0].kind == Line::Release && all[0].text == "1.1.0 · 2026-10-10");
    CHECK(all[1].kind == Line::Section && all[1].text == "Features");
    CHECK(all[2].kind == Line::Item && all[2].text == "ui: an About tab with the changelog" && all[2].depth == 0);
    CHECK(all[3].kind == Line::Item && all[3].text == "a long item that goes on");
    CHECK(all[4].kind == Line::Item && all[4].text == "a nested one" && all[4].depth == 1);
    CHECK(all[5].kind == Line::Section && all[5].text == "Changes");
    CHECK(all[6].kind == Line::Item && all[6].text == "uses romfs and borealis");
    CHECK(all[7].kind == Line::Release && all[7].text == "1.0.0 · 2026-09-01");
    CHECK(all[8].kind == Line::Text && all[8].text == "First release.");
    CHECK(all[9].kind == Line::Release && all[9].text == "Upstream history");
    CHECK(all[10].kind == Line::Section && all[10].text == "Pctl Manager v3.0.0");

    // At most two releases: the upstream history is left out.
    const auto two = changelog::parse(MD, 2);
    CHECK(two.size() == 9 && two.back().text == "First release.");

    // Text that only looks like a link or a reference stays.
    const auto plain = changelog::parse("* see [docs] and (abc) and ([not a ref](u))\n", 0);
    CHECK(plain.size() == 1 && plain[0].text == "see [docs] and (abc) and (not a ref)");

    // Only the running version's notes.
    std::string date;
    const auto notes = changelog::release_notes(MD, "1.1.0", &date);
    CHECK(date == "2026-10-10");
    CHECK(notes.size() == 6 && notes[0].text == "Features" && notes.back().text == "uses romfs and borealis");
    const auto first = changelog::release_notes(MD, "v1.0.0", &date);
    CHECK(date == "2026-09-01" && first.size() == 1 && first[0].text == "First release.");
    date = "unchanged";
    CHECK(changelog::release_notes(MD, "1.2.0", &date).empty() && date == "unchanged");
    CHECK(changelog::release_notes(MD, "1.1", nullptr).empty());
    CHECK(changelog::release_notes(MD, "", nullptr).empty());

    CHECK(changelog::parse("", 0).empty());
    CHECK(changelog::parse("# Changelog\n", 0).empty());

    return CHECK_DONE("changelog parsing, links, references and emoji assertions passed");
}
