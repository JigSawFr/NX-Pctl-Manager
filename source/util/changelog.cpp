// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/changelog.hpp"

#include <cctype>
#include <cstdint>
#include <sstream>

namespace changelog
{
namespace
{
std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool starts_with(const std::string& s, const char* prefix)
{
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

// Pictographs, dingbats and the joiners / variation selectors that go with
// them: the console's shared font draws none of them.
bool is_emoji(uint32_t cp)
{
    return cp >= 0x1F000 || (cp >= 0x2600 && cp <= 0x27BF) || (cp >= 0x2B00 && cp <= 0x2BFF) ||
           cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F);
}

std::string drop_emoji(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        uint32_t cp = n == 1 ? c : n == 2 ? c & 0x1F : n == 3 ? c & 0x0F : c & 0x07;
        for (size_t k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        if (!is_emoji(cp)) out.append(s, i, n);
        i += n;
    }
    return out;
}

// "abc1234" (a commit) or "#12" (a pull request or an issue).
bool is_reference(const std::string& s)
{
    if (s.size() > 1 && s[0] == '#') {
        for (size_t i = 1; i < s.size(); i++)
            if (!std::isdigit((unsigned char)s[i])) return false;
        return true;
    }
    if (s.size() < 7 || s.size() > 40) return false;
    for (char c : s)
        if (!std::isxdigit((unsigned char)c)) return false;
    return true;
}

// "[text](url)" → "text"; " ([#12](url))" and " ([abc1234](url))" go.
std::string strip_links(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '[') {
            const size_t close = s.find(']', i);
            if (close != std::string::npos && close + 1 < s.size() && s[close + 1] == '(') {
                const size_t end = s.find(')', close);
                if (end != std::string::npos) {
                    const std::string text = s.substr(i + 1, close - i - 1);
                    // A reference wrapped in parentheses: drop the whole group.
                    if (is_reference(text) && !out.empty() && out.back() == '(' && end + 1 < s.size() && s[end + 1] == ')') {
                        out.pop_back();
                        while (!out.empty() && out.back() == ' ') out.pop_back();
                        i = end + 2;
                    } else {
                        out += text;
                        i = end + 1;
                    }
                    continue;
                }
            }
        }
        out += s[i++];
    }
    return out;
}

std::string clean(const std::string& s)
{
    std::string out;
    for (char c : strip_links(drop_emoji(s)))
        if (c != '`') out += c;
    for (size_t p; (p = out.find("**")) != std::string::npos;) out.erase(p, 2);
    return trim(out);
}

// "1.1.0 (2026-10-10)" → "1.1.0 · 2026-10-10".
std::string release_title(std::string s)
{
    const size_t open = s.rfind(" (");
    if (open != std::string::npos && s.back() == ')')
        s = s.substr(0, open) + " · " + s.substr(open + 2, s.size() - open - 3);
    return s;
}
}   // namespace

std::vector<Line> parse(const std::string& md, int max_releases)
{
    std::vector<Line> lines;
    std::istringstream in(md);
    std::string raw;
    int  releases = 0;
    bool in_item  = false;   // an indented line continues the last item
    while (std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        const std::string t = trim(raw);
        if (t.empty()) {
            in_item = false;
            continue;
        }
        if (starts_with(t, "## ")) {
            if (max_releases > 0 && ++releases > max_releases) break;
            lines.push_back({ Line::Release, release_title(clean(t.substr(3))) });
            in_item = false;
        } else if (starts_with(t, "# ")) {
            in_item = false;   // the document's title
        } else if (starts_with(t, "### ")) {
            lines.push_back({ Line::Section, clean(t.substr(4)) });
            in_item = false;
        } else if (starts_with(t, "* ") || starts_with(t, "- ")) {
            const size_t indent = raw.find_first_not_of(' ');
            lines.push_back({ Line::Item, clean(t.substr(2)), (int)(indent / 2) });
            in_item = true;
        } else if (in_item && raw[0] == ' ') {
            lines.back().text += " " + clean(t);
        } else {
            lines.push_back({ Line::Text, clean(t) });
            in_item = false;
        }
        if (!lines.empty() && lines.back().text.empty()) lines.pop_back();
    }
    return lines;
}

std::vector<Line> release_notes(const std::string& md, const std::string& version, std::string* date)
{
    auto bare = [](const std::string& v) { return !v.empty() && (v[0] == 'v' || v[0] == 'V') ? v.substr(1) : v; };
    const std::string wanted = bare(version);
    std::vector<Line> notes;
    bool inside = false;
    for (Line& l : parse(md, 0)) {
        if (l.kind == Line::Release) {
            if (inside) break;
            const size_t dot = l.text.find(" · ");
            if (wanted.empty() || bare(l.text.substr(0, dot)) != wanted) continue;
            inside = true;
            if (date) *date = dot == std::string::npos ? "" : l.text.substr(dot + std::string(" · ").size());
        } else if (inside) {
            notes.push_back(std::move(l));
        }
    }
    return notes;
}

}   // namespace changelog
