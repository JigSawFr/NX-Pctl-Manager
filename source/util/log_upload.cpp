// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/log_upload.hpp"

#include <algorithm>
#include <cctype>

#include "util/paths.hpp"

namespace log_upload
{

std::vector<Part> debug_files()
{
    const std::pair<const char*, std::string> files[] = {
        { "logs/play_timer_block.json", paths::logs_dir() + "/play_timer_block.json" },
        { "history.json", paths::history_file() },
        { "config.json", paths::config_file() },
    };
    std::vector<Part> out;
    for (const auto& f : files) {
        std::string content;
        if (paths::read_file(f.second, content)) out.push_back({ f.first, content });
    }
    return out;
}

std::vector<std::string> saved_reports(size_t limit)
{
    // diagnostic::save names them by date ("20261009_141203[_n].txt"): the
    // names sort by age. Other .txt files of logs/ are not reports.
    std::vector<std::string> names;
    for (const std::string& name : paths::list_files(paths::logs_dir(), ".txt"))
        if (name.size() >= 19 && std::all_of(name.begin(), name.begin() + 8, ::isdigit) && name[8] == '_')
            names.push_back(name);
    std::sort(names.rbegin(), names.rend());
    if (names.size() > limit) names.resize(limit);
    return names;
}

std::string bundle(const std::vector<Part>& parts, size_t max_bytes, bool* truncated)
{
    static const std::string NOTICE = "\n\n(cut: too large to send whole)\n";
    std::string out;
    for (const Part& p : parts) {
        if (!out.empty()) out += "\n";
        out += "===== " + p.name + " =====\n" + p.content;
        if (!p.content.empty() && p.content.back() != '\n') out += "\n";
    }
    if (truncated) *truncated = false;
    if (out.size() <= max_bytes) return out;
    if (truncated) *truncated = true;
    size_t keep = max_bytes > NOTICE.size() ? max_bytes - NOTICE.size() : 0;
    // Back to the start of a character: never half of a UTF-8 sequence.
    while (keep > 0 && ((unsigned char)out[keep] & 0xC0) == 0x80) keep--;
    out.resize(keep);
    return out + NOTICE.substr(0, max_bytes - keep);
}

bool parse_reply(const std::string& body, std::string* url)
{
    static const std::string PREFIX = "https://dpaste.org/";
    size_t b = 0, e = body.size();
    while (b < e && std::isspace((unsigned char)body[b])) b++;
    while (e > b && std::isspace((unsigned char)body[e - 1])) e--;
    const std::string text = body.substr(b, e - b);
    if (text.compare(0, PREFIX.size(), PREFIX) != 0) return false;
    const std::string id = text.substr(PREFIX.size());
    if (id.empty() || id.size() > 32 || !std::all_of(id.begin(), id.end(), ::isalnum)) return false;
    *url = text;
    return true;
}

std::string url_encode(const std::string& text)
{
    static const char HEX[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0xF];
        }
    }
    return out;
}

std::string issue_url(const std::string& repo_url, const std::string& paste_url, const std::string& version,
                      const std::string& firmware, const std::string& atmosphere)
{
    // Issue forms take each field's id as a query parameter.
    std::string url = repo_url + "/issues/new?template=1-bug.yml&report=" + url_encode(paste_url);
    if (!version.empty()) url += "&version=" + url_encode(version);
    if (!firmware.empty()) url += "&firmware=" + url_encode(firmware);
    if (!atmosphere.empty()) url += "&atmosphere=" + url_encode(atmosphere);
    return url;
}

std::string form(const std::string& text)
{
    // "_text": shown as plain text, never highlighted as code.
    return "content=" + url_encode(text) + "&format=url&lexer=_text&expires=" + std::to_string(EXPIRES_S);
}

std::string short_url(const std::string& url)
{
    const size_t scheme = url.find("://");
    return scheme == std::string::npos ? url : url.substr(scheme + 3);
}

}   // namespace log_upload
