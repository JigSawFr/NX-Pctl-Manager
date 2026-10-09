// log_upload — sends a diagnostic report, with the developer tools' debug
// files, online and gives back a short link, so a report reaches whoever
// helps (a bug report, a chat) without taking the SD card out. The link also
// prefills the bug-report form of the repository. Two places:
// - bpa.st, a public pastebin (pinnwand) that deletes it after one month;
// - a secret gist in the parent's GitHub account, when PlayGuard is signed in
//   to GitHub (github_auth.hpp): it stays until deleted on GitHub.
// (dpaste.org, used before, sits behind Cloudflare, which refuses the Switch.)
//
// What is sent is only what the parent picked, after a confirmation that
// says where it goes: the diagnostic report never holds the PIN or the
// serial number, and the debug files are PlayGuard's own settings and change
// history. Anyone with the link can read it.
//
// Everything but upload() is plain C++ for the host tests (tests/log_upload);
// upload() is in log_upload_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace log_upload
{

enum class Host
{
    Bpaste,   // bpa.st
    Gist,     // a secret gist, with the GitHub token
};

// The most each place takes. bpa.st checks its page against 256 KiB: the
// text as HTML with line numbers, about 1.5 times longer. A report is about
// 30 KiB; the change history is what grows.
constexpr size_t BPASTE_MAX_BYTES = 128 * 1024;
constexpr size_t GIST_MAX_BYTES = 512 * 1024;
inline size_t max_bytes(Host host)
{
    return host == Host::Gist ? GIST_MAX_BYTES : BPASTE_MAX_BYTES;
}

// How long bpa.st keeps a paste (one of the durations it accepts).
constexpr const char* BPASTE_EXPIRY = "1month";

struct Part
{
    std::string name;      // "diagnostic report", "logs/play_timer_block.json"
    std::string content;
};

// The debug files found in the data directory, in a fixed order:
// logs/play_timer_block.json (Developer tools › play-timer block),
// history.json (the change history) and config.json (PlayGuard's settings;
// the PIN is not one of them). Missing ones are left out. Never the GitHub
// token (github_auth.hpp keeps it in a file of its own).
std::vector<Part> debug_files();

// Names of the reports saved in logs/ ("20261009_141203.txt"), newest first,
// at most `limit`.
std::vector<std::string> saved_reports(size_t limit = 10);

// The parts one after the other, each under a "===== name =====" line. Cut
// on a whole UTF-8 character to at most `max_bytes` (a closing notice
// included), and *truncated set, when they do not fit.
std::string bundle(const std::vector<Part>& parts, size_t max_bytes, bool* truncated = nullptr);

// bpa.st's answer (its /curl endpoint):
//   "Paste URL:   https://bpa.st/ABCD\nRaw URL: …\nRemoval URL: https://bpa.st/remove/EFGH\n".
// True, and *url and *removal set, only for such links.
bool parse_bpaste(const std::string& body, std::string* url, std::string* removal);

// GitHub's answer to a new gist: true, and *url set, when its html_url is a
// gist.github.com link.
bool parse_gist(const std::string& body, std::string* url);

// RFC 3986 percent-encoding of everything but unreserved characters.
std::string url_encode(const std::string& text);

// The repository's bug-report form (.github/ISSUE_TEMPLATE/1-bug.yml) with
// the report's link and what the console knows already filled in.
std::string issue_url(const std::string& repo_url, const std::string& paste_url, const std::string& version,
                      const std::string& firmware, const std::string& atmosphere);

// The form body sent to bpa.st (multipart/form-data, the text as is): the
// text, plain text, the expiry. *content_type gets the type with its
// boundary, one the text does not hold.
std::string form(const std::string& text, std::string* content_type);

// The JSON body of a new secret gist holding `text`.
std::string gist_body(const std::string& text, const std::string& version);

// "https://bpa.st/ABCD" -> "bpa.st/ABCD": what to type by hand.
std::string short_url(const std::string& url);

struct Result
{
    bool        ok = false;
    std::string url;
    std::string removal;           // bpa.st: the link that deletes the paste
    std::string error;             // why it failed, in English
    bool        relogin = false;   // GitHub refused the token: sign in again (the gist permission)
};

// Sends `text` to `host` (a gist with `token`). Blocking (network): run it
// with brls::async. On the desktop build PLAYGUARD_SIM_PASTE ("offline", or
// the link to answer) replaces the network.
Result upload(const std::string& text, Host host, const std::string& token = "", const std::string& version = "");

}   // namespace log_upload
