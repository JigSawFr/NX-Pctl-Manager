// log_upload — sends a diagnostic report, with the developer tools' debug
// files, to a public pastebin (dpaste.org, kept 30 days) and gives back a short link, so a
// report reaches whoever helps (a bug report, a chat) without taking the SD
// card out. The link also prefills the bug-report form of the repository.
//
// What is sent is only what the parent picked, after a confirmation that
// says where it goes: the diagnostic report never holds the PIN or the
// serial number, and the debug files are PlayGuard's own settings and change
// history. Anyone with the link can read the paste.
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

// dpaste.org refuses a request much over 500 KiB; the text is sent
// percent-encoded (up to three times longer), and a report is about 30 KiB.
constexpr size_t MAX_BYTES = 160 * 1024;

// How long dpaste.org keeps a paste, in seconds (one of the durations it accepts).
constexpr long EXPIRES_S = 30 * 24 * 3600;

struct Part
{
    std::string name;      // "diagnostic report", "logs/play_timer_block.json"
    std::string content;
};

// The end of logs/play_timer_log.csv sent, at most (util/pt_log.hpp).
constexpr size_t PT_LOG_BYTES = 48 * 1024;

// The debug files found in the data directory, in a fixed order:
// logs/play_timer_block.json (Developer tools › play-timer block),
// logs/play_timer_log.csv (Developer tools › record the play timer: its last
// PT_LOG_BYTES, header kept), history.json (the change history) and config.json (PlayGuard's settings;
// the PIN is not one of them). Missing ones are left out. Never the GitHub
// token (github_auth.hpp keeps it in a file of its own).
std::vector<Part> debug_files();

// Names of the reports saved in logs/ ("20261009_141203.txt"), newest first,
// at most `limit`.
std::vector<std::string> saved_reports(size_t limit = 10);

// The parts one after the other, each under a "===== name =====" line. Cut
// on a whole UTF-8 character to at most `max_bytes` (a closing notice
// included), and *truncated set, when they do not fit.
std::string bundle(const std::vector<Part>& parts, size_t max_bytes = MAX_BYTES, bool* truncated = nullptr);

// dpaste.org answers the link as the body: "https://dpaste.org/AbC1\n".
// True, and *url set, only for such a link (letters and digits as the id).
bool parse_reply(const std::string& body, std::string* url);

// RFC 3986 percent-encoding of everything but unreserved characters.
std::string url_encode(const std::string& text);

// The repository's bug-report form (.github/ISSUE_TEMPLATE/1-bug.yml) with
// the report's link and what the console knows already filled in.
std::string issue_url(const std::string& repo_url, const std::string& paste_url, const std::string& version,
                      const std::string& firmware, const std::string& atmosphere);

// The form body of the upload: the text, the link as the answer, the expiry.
std::string form(const std::string& text);

// "https://dpaste.org/AbC1" -> "dpaste.org/AbC1": what to type by hand.
std::string short_url(const std::string& url);

struct Result
{
    bool        ok = false;
    std::string url;
    std::string error;             // why it failed, in English
};

// POSTs `text` to dpaste.org. Blocking (network): run it with brls::async. On
// the desktop build PLAYGUARD_SIM_PASTE ("offline", or the link to answer)
// replaces the network.
Result upload(const std::string& text);

}   // namespace log_upload
