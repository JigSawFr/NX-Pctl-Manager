// dev_builds — install another build of PlayGuard in place, from the
// developer tools: the latest release, one of the last commits of main, or
// the build of an open pull request.
//
// CI (.github/workflows/build.yml, job dev-builds) publishes them as GitHub
// pre-releases, readable without an account:
//   tag "dev":   playguard-<commit>.nro for the last commits of main;
//   tag "pr-<n>": playguard-<commit>.nro, the latest build of pull request n
//                 (from this repository only; removed when it closes).
// Pre-releases are never "latest": the update check and the stores keep to
// the releases.
//
// The download is checked (size, the SHA-256 GitHub records for the asset,
// the NRO header) before it replaces the running .nro, which hbloader loaded
// whole into memory, so it can be overwritten; then hbloader starts it.
//
// Everything but fetch() and download() is plain C++ for the host tests
// (tests/dev_builds); those two are in dev_builds_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dev_builds
{

enum class Kind
{
    Release,       // the latest release (playguard.nro)
    Main,          // a commit of main
    PullRequest,   // the latest build of a pull request
};

struct Build
{
    Kind        kind = Kind::Main;
    std::string version;   // Release: "1.2.0"
    int         pr = 0;    // PullRequest: its number
    std::string title;     // PullRequest: its title
    std::string commit;    // Main / PullRequest: 7 hex digits
    std::string date;      // "2026-10-09T12:03:00Z", when the asset was uploaded
    std::string url;       // browser_download_url
    uint64_t    size = 0;
    std::string sha256;    // 64 hex digits; empty when GitHub gave no digest
};

// "playguard-1a2b3c4.nro" -> "1a2b3c4"; empty for any other name.
std::string commit_of(const std::string& asset_name);

// One release object of the GitHub API (GET /repos/…/releases/…) into the
// builds it holds: "dev" gives one per commit, "pr-<n>" its newest, a
// release that is not a pre-release its playguard.nro. False for anything
// else (no builds, malformed).
bool parse_release(const std::string& json, std::vector<Build>* out);
// The same for an array of them (GET /repos/…/releases).
bool parse_releases(const std::string& json, std::vector<Build>* out);

// Release first, then main newest first, then pull requests by number,
// highest first; the same build twice (same kind and commit) once.
void sort(std::vector<Build>* builds);

// "sha256:<64 hex>" (GitHub's asset digest) -> the 64 hex digits, lower
// case; empty when malformed.
std::string digest_hex(const std::string& digest);

// An NRO: "NRO0" at offset 0x10.
bool is_nro(const std::string& head);

// Checks the downloaded file against `b` (size, SHA-256 when known, NRO
// header). False, and *error set (in English), when it does not match.
bool verify(const std::string& path, const Build& b, std::string* error);

// Replaces `target` with `fresh` (both on the same file system): target
// moves aside to "<target>.old", fresh takes its name, then the old one is
// removed. When fresh cannot take its place, the old one is put back. A
// missing target is simply created.
bool replace(const std::string& target, const std::string& fresh, std::string* error);

// The builds that can be installed, newest first (see sort()). Blocking
// (network): run it with brls::async. On the desktop build
// PLAYGUARD_SIM_DEV_BUILDS ("offline", or a JSON file holding a release
// array) replaces the network.
bool fetch(std::vector<Build>* out, std::string* error);

// Downloads `b` to `path` and verifies it. Blocking. On the desktop build,
// with PLAYGUARD_SIM_DEV_BUILDS set, the URL is a local file to copy.
bool download(const Build& b, const std::string& path, std::string* error);

}   // namespace dev_builds
