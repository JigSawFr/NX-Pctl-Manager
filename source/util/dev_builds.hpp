// dev_builds — install another build of PlayGuard in place, from the
// developer tools: the latest release, one of the last commits of main, or
// the newest build of an open pull request (from this repository: a fork's
// build is never offered, nobody with write access has reviewed its code).
//
// The release is the playguard.nro of the latest GitHub release, readable
// without an account. The others are the playguard_release artifact the
// build workflow keeps for every run (.github/workflows/build.yml): a zip
// holding playguard.nro, which GitHub only hands to a signed-in user, hence
// the token of util/github_auth.hpp. Artifacts expire (90 days by default).
//
// The download is checked (size and the SHA-256 GitHub records; for an
// artifact, the zip's, then the CRC-32 of playguard.nro inside it; the NRO
// header) before it replaces the running .nro, which hbloader loaded whole
// into memory, so it can be overwritten; then hbloader starts it.
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
    PullRequest,   // the newest build of an open pull request
};

struct Build
{
    Kind        kind = Kind::Main;
    bool        artifact = false;   // url, size and sha256 are those of a zip holding playguard.nro
    std::string version;   // Release: "1.2.0"
    int         pr = 0;    // PullRequest: its number
    std::string title;     // PullRequest: its title
    std::string commit;    // Main / PullRequest: 7 hex digits
    std::string date;      // "2026-10-09T12:03:00Z", when it was built
    std::string url;       // the download (an artifact's archive_download_url needs the token)
    uint64_t    size = 0;
    std::string sha256;    // 64 hex digits; empty when GitHub gave no digest
};

// One run's artifact, as GET /repos/…/actions/artifacts lists it.
struct Artifact
{
    std::string sha256, url, date, branch, commit;   // commit: the run's head, 7 hex digits
    uint64_t    size = 0;
    int64_t     repo = 0, head_repo = 0;   // the run's repository and the head's (a fork's for its PRs)
};

// An open pull request (GET /repos/…/pulls).
struct Pull
{
    int         number = 0;
    std::string title, branch;
    int64_t     head_repo = 0;
};

// The latest release (GET /repos/…/releases/latest): its playguard.nro.
// False for anything else (a pre-release, no such asset, malformed).
bool parse_release(const std::string& json, Build* out);
// The "playguard_release" artifacts that have not expired, newest first as listed.
bool parse_artifacts(const std::string& json, std::vector<Artifact>* out);
bool parse_pulls(const std::string& json, std::vector<Pull>* out);

// The builds: main's newest `keep_main` commits (pushed to this repository's
// main), then for each open pull request from a branch of this repository
// the newest artifact of that branch (pull requests from forks are left out). Newest first within each kind; one per commit.
std::vector<Build> combine(const std::vector<Artifact>& artifacts, const std::vector<Pull>& pulls,
                           size_t keep_main = 20);

// "sha256:<64 hex>" (GitHub's digest) -> the 64 hex digits, lower case;
// empty when malformed.
std::string digest_hex(const std::string& digest);

// An NRO: "NRO0" at offset 0x10.
bool is_nro(const std::string& head);

// Checks a downloaded file against `size` and `sha256` (when known).
bool verify_file(const std::string& path, uint64_t size, const std::string& sha256, std::string* error);
// Checks that `path` is an NRO.
bool verify_nro(const std::string& path, std::string* error);

// Replaces `target` with `fresh` (both on the same file system): target
// moves aside to "<target>.old", fresh takes its name, then the old one is
// removed. When fresh cannot take its place, the old one is put back. A
// missing target is simply created.
bool replace(const std::string& target, const std::string& fresh, std::string* error);

// The last list fetched, kept on the SD card (cache/dev_builds.json) so the
// next opening shows it at once while it is fresh.
struct Cache
{
    std::vector<Build> builds;
    bool    needs_login = false;   // as fetch() set it: signing in or out makes the list stale
    int64_t fetched_at = 0;        // POSIX seconds
};

// A list older than this is fetched again before it is shown.
constexpr int64_t CACHE_MAX_AGE_S = 10 * 60;

std::string encode_cache(const Cache& cache);
// False for anything else (damaged, another layout); a build that does not
// look like one (no https URL, a malformed commit or digest) is left out.
bool decode_cache(const std::string& json, Cache* out);
// Fresh: fetched less than CACHE_MAX_AGE_S before `now` (not in the future:
// the clock was set back), in the same sign-in state.
bool cache_fresh(const Cache& cache, int64_t now, bool needs_login);

// The builds that can be installed: the release, then (with a token) main
// and the pull requests. Blocking (network): run it with brls::async.
// *needs_login is set when there is no token, so only the release is listed.
// On the desktop build PLAYGUARD_SIM_DEV_BUILDS ("offline", or a folder
// holding latest.json, artifacts.json and pulls.json) replaces the network.
bool fetch(std::vector<Build>* out, bool* needs_login, std::string* error);

// Downloads `b` to `path` (playguard.nro itself, out of the zip for an
// artifact) and verifies it. Blocking. On the desktop build, with
// PLAYGUARD_SIM_DEV_BUILDS set, a URL "https://<path>" is a local file.
bool download(const Build& b, const std::string& path, std::string* error);

}   // namespace dev_builds
