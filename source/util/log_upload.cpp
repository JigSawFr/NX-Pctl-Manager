// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/log_upload.hpp"

#include <algorithm>
#include <cctype>

#include <borealis/extern/nlohmann/json.hpp>

#include "util/paths.hpp"
#include "util/pt_log.hpp"

namespace log_upload
{

std::vector<Part> debug_files()
{
    std::vector<Part> out;
    std::string content;
    if (paths::read_file(paths::logs_dir() + "/play_timer_block.json", content))
        out.push_back({ "logs/play_timer_block.json", content });
    // The recorder's file can reach megabytes: its last lines only.
    if (paths::read_file(pt_log::path(), content))
        out.push_back({ "logs/play_timer_log.csv", pt_log::tail(content, PT_LOG_BYTES) });
    if (paths::read_file(paths::history_file(), content)) out.push_back({ "history.json", content });
    if (paths::read_file(paths::config_file(), content)) out.push_back({ "config.json", content });
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

namespace
{
// "https://<host>/<id>" with the id made only of the given characters.
bool link_with_id(const std::string& text, const std::string& prefix, bool (*ok)(unsigned char))
{
    if (text.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string id = text.substr(prefix.size());
    return !id.empty() && id.size() <= 64 &&
           std::all_of(id.begin(), id.end(), [ok](char c) { return ok((unsigned char)c); });
}

bool alnum(unsigned char c)
{
    return std::isalnum(c) != 0;
}

std::string trimmed(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

// The value after "<label>:" on its own line of `body`, trimmed.
std::string field(const std::string& body, const std::string& label)
{
    size_t at = 0;
    while (at < body.size()) {
        size_t end = body.find('\n', at);
        if (end == std::string::npos) end = body.size();
        const std::string line = body.substr(at, end - at);
        if (line.compare(0, label.size() + 1, label + ":") == 0) return trimmed(line.substr(label.size() + 1));
        at = end + 1;
    }
    return "";
}
}   // namespace

bool parse_bpaste(const std::string& body, std::string* url, std::string* removal)
{
    const std::string link = field(body, "Paste URL");
    const std::string remove = field(body, "Removal URL");
    if (!link_with_id(link, "https://bpa.st/", alnum) || !link_with_id(remove, "https://bpa.st/remove/", alnum))
        return false;
    *url = link;
    *removal = remove;
    return true;
}

bool parse_gist(const std::string& body, std::string* url)
{
    try {
        const auto j = nlohmann::json::parse(body);
        const auto it = j.is_object() ? j.find("html_url") : j.end();
        if (it == j.end() || !it->is_string()) return false;
        // "https://gist.github.com/<id>", or ".../<user>/<id>".
        const std::string link = it->get<std::string>();
        if (!link_with_id(link, "https://gist.github.com/", [](unsigned char c) {
                return std::isalnum(c) || c == '-' || c == '/';
            }) || link.find("//", 8) != std::string::npos || link.back() == '/')
            return false;
        *url = link;
        return true;
    } catch (const std::exception&) {
        return false;
    }
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

std::string form(const std::string& text, std::string* content_type)
{
    std::string boundary = "PlayGuardBoundary";
    for (int n = 0; text.find(boundary) != std::string::npos; n++) boundary = "PlayGuardBoundary" + std::to_string(n);
    *content_type = "multipart/form-data; boundary=" + boundary;
    std::string out;
    auto add = [&](const char* name, const std::string& value) {
        out += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n" + value + "\r\n";
    };
    add("raw", text);
    add("lexer", "text");   // shown as plain text, never highlighted as code
    add("expiry", BPASTE_EXPIRY);
    return out + "--" + boundary + "--\r\n";
}

std::string gist_body(const std::string& text, const std::string& version)
{
    nlohmann::json j;
    j["description"] = "PlayGuard " + version + " diagnostic report";
    j["public"] = false;
    j["files"]["playguard-report.txt"]["content"] = text;
    // A cut can never split a character (bundle()); anything else invalid is replaced.
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string short_url(const std::string& url)
{
    const size_t scheme = url.find("://");
    return scheme == std::string::npos ? url : url.substr(scheme + 3);
}

}   // namespace log_upload
