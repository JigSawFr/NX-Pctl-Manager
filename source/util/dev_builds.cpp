// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/dev_builds.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <set>

#include <borealis/extern/nlohmann/json.hpp>

#include "util/sha256.hpp"

namespace dev_builds
{

namespace
{
using nlohmann::json;

bool is_hex(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isxdigit((unsigned char)c) != 0; });
}

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string str(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

int64_t integer(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number_integer() ? it->get<int64_t>() : 0;
}

// A full commit hash -> its first 7 digits; empty when it is not one.
std::string short_commit(const std::string& sha)
{
    return sha.size() >= 7 && sha.size() <= 64 && is_hex(sha) ? lower(sha.substr(0, 7)) : "";
}

bool https(const std::string& url) { return url.compare(0, 8, "https://") == 0; }

template <typename F>
bool parse(const std::string& text, F f)
{
    try {
        return f(json::parse(text));
    } catch (const std::exception&) {
        return false;
    }
}
}   // namespace

bool parse_release(const std::string& text, Build* out)
{
    return parse(text, [out](const json& r) {
        if (!r.is_object() || r.value("prerelease", true) || r.value("draft", true)) return false;
        if (!r.contains("assets") || !r["assets"].is_array()) return false;
        const std::string tag = str(r, "tag_name");
        for (const auto& a : r["assets"]) {
            if (!a.is_object() || str(a, "name") != "playguard.nro") continue;
            Build b;
            b.kind = Kind::Release;
            b.version = tag.size() > 1 && tag[0] == 'v' ? tag.substr(1) : tag;
            b.url = str(a, "browser_download_url");
            b.date = str(a, "updated_at");
            b.sha256 = digest_hex(str(a, "digest"));
            const int64_t size = integer(a, "size");
            if (b.version.empty() || !https(b.url) || size <= 0) return false;
            b.size = (uint64_t)size;
            *out = b;
            return true;
        }
        return false;
    });
}

bool parse_artifacts(const std::string& text, std::vector<Artifact>* out)
{
    return parse(text, [out](const json& j) {
        if (!j.is_object() || !j.contains("artifacts") || !j["artifacts"].is_array()) return false;
        for (const auto& a : j["artifacts"]) {
            if (!a.is_object() || str(a, "name") != "playguard_release" || a.value("expired", true)) continue;
            if (!a.contains("workflow_run") || !a["workflow_run"].is_object()) continue;
            const auto& run = a["workflow_run"];
            Artifact x;
            x.url = str(a, "archive_download_url");
            x.date = str(a, "created_at");
            x.sha256 = digest_hex(str(a, "digest"));
            x.branch = str(run, "head_branch");
            x.commit = short_commit(str(run, "head_sha"));
            x.repo = integer(run, "repository_id");
            x.head_repo = integer(run, "head_repository_id");
            const int64_t size = integer(a, "size_in_bytes");
            if (!https(x.url) || size <= 0 || x.commit.empty() || x.branch.empty() || !x.repo || !x.head_repo) continue;
            x.size = (uint64_t)size;
            out->push_back(x);
        }
        return true;
    });
}

bool parse_pulls(const std::string& text, std::vector<Pull>* out)
{
    return parse(text, [out](const json& j) {
        if (!j.is_array()) return false;
        for (const auto& p : j) {
            if (!p.is_object() || !p.contains("head") || !p["head"].is_object()) continue;
            const auto& head = p["head"];
            Pull x;
            x.number = (int)integer(p, "number");
            x.title = str(p, "title");
            x.branch = str(head, "ref");
            // A fork that was deleted has no repository: its builds cannot be matched.
            x.head_repo = head.contains("repo") && head["repo"].is_object() ? integer(head["repo"], "id") : 0;
            if (x.number > 0 && !x.branch.empty() && x.head_repo) out->push_back(x);
        }
        return true;
    });
}

std::vector<Build> combine(const std::vector<Artifact>& artifacts, const std::vector<Pull>& pulls, size_t keep_main)
{
    std::vector<Artifact> sorted = artifacts;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Artifact& a, const Artifact& b) { return a.date > b.date; });
    auto build = [](const Artifact& a, Kind kind) {
        Build b;
        b.kind = kind;
        b.artifact = true;
        b.commit = a.commit;
        b.date = a.date;
        b.url = a.url;
        b.size = a.size;
        b.sha256 = a.sha256;
        return b;
    };
    std::vector<Build> out;
    std::set<std::string> seen;
    for (const Artifact& a : sorted) {
        if (out.size() >= keep_main) break;
        // Pushed to this repository's main (a fork's "main" is a pull request's branch).
        if (a.branch == "main" && a.head_repo == a.repo && seen.insert(a.commit).second) out.push_back(build(a, Kind::Main));
    }
    std::vector<Pull> by_number = pulls;
    std::sort(by_number.begin(), by_number.end(), [](const Pull& a, const Pull& b) { return a.number > b.number; });
    for (const Pull& p : by_number) {
        for (const Artifact& a : sorted) {
            if (a.branch != p.branch || a.head_repo != p.head_repo) continue;
            Build b = build(a, Kind::PullRequest);
            b.pr = p.number;
            b.title = p.title;
            out.push_back(b);
            break;   // the newest only
        }
    }
    return out;
}

namespace
{
const char* kind_name(Kind k)
{
    switch (k) {
        case Kind::Release: return "release";
        case Kind::Main: return "main";
        case Kind::PullRequest: return "pr";
    }
    return "";
}
}   // namespace

std::string encode_cache(const Cache& cache)
{
    json list = json::array();
    for (const Build& b : cache.builds)
        list.push_back({ { "kind", kind_name(b.kind) }, { "artifact", b.artifact }, { "version", b.version },
                         { "pr", b.pr }, { "title", b.title }, { "commit", b.commit }, { "date", b.date },
                         { "url", b.url }, { "size", b.size }, { "sha256", b.sha256 } });
    const json j = { { "version", 1 }, { "fetched_at", cache.fetched_at }, { "needs_login", cache.needs_login },
                     { "builds", list } };
    return j.dump(1) + "\n";
}

bool decode_cache(const std::string& text, Cache* out)
{
    return parse(text, [out](const json& j) {
        if (!j.is_object() || integer(j, "version") != 1 || !j.contains("builds") || !j["builds"].is_array()) return false;
        Cache c;
        c.fetched_at = integer(j, "fetched_at");
        c.needs_login = j.value("needs_login", true);
        for (const auto& x : j["builds"]) {
            if (!x.is_object()) continue;
            Build b;
            const std::string kind = str(x, "kind");
            if (kind == "release") b.kind = Kind::Release;
            else if (kind == "main") b.kind = Kind::Main;
            else if (kind == "pr") b.kind = Kind::PullRequest;
            else continue;
            b.artifact = x.value("artifact", false);
            b.version = str(x, "version");
            b.pr = (int)integer(x, "pr");
            b.title = str(x, "title");
            b.commit = str(x, "commit");
            b.date = str(x, "date");
            b.url = str(x, "url");
            b.sha256 = str(x, "sha256");
            const int64_t size = integer(x, "size");
            const bool commit_ok = b.kind == Kind::Release ? b.commit.empty() : short_commit(b.commit) == b.commit;
            const bool sha_ok = b.sha256.empty() || (b.sha256.size() == 64 && is_hex(b.sha256) && lower(b.sha256) == b.sha256);
            if (!https(b.url) || size <= 0 || !commit_ok || !sha_ok) continue;
            if (b.kind == Kind::Release ? b.version.empty() : !b.artifact) continue;
            if (b.kind == Kind::PullRequest && b.pr <= 0) continue;
            b.size = (uint64_t)size;
            c.builds.push_back(b);
        }
        *out = c;
        return true;
    });
}

bool cache_fresh(const Cache& cache, int64_t now, bool needs_login)
{
    const int64_t age = now - cache.fetched_at;
    return cache.fetched_at > 0 && age >= 0 && age < CACHE_MAX_AGE_S && cache.needs_login == needs_login &&
           !cache.builds.empty();
}

std::string digest_hex(const std::string& digest)
{
    static const std::string PREFIX = "sha256:";
    if (digest.size() != PREFIX.size() + 64 || digest.compare(0, PREFIX.size(), PREFIX) != 0) return "";
    const std::string hex = digest.substr(PREFIX.size());
    return is_hex(hex) ? lower(hex) : "";
}

bool is_nro(const std::string& head)
{
    return head.size() >= 0x14 && head.compare(0x10, 4, "NRO0") == 0;
}

bool verify_file(const std::string& path, uint64_t size, const std::string& sha256, std::string* error)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "cannot read the download";
        return false;
    }
    Sha256 hash;
    uint64_t got = 0;
    char buf[16 * 1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        hash.update(buf, n);
        got += n;
    }
    const bool read_error = std::ferror(f) != 0;
    std::fclose(f);
    if (read_error) {
        if (error) *error = "cannot read the download";
        return false;
    }
    if (got != size) {
        if (error) *error = "size " + std::to_string(got) + " instead of " + std::to_string(size);
        return false;
    }
    if (!sha256.empty() && hash.hex() != sha256) {
        if (error) *error = "SHA-256 does not match";
        return false;
    }
    return true;
}

bool verify_nro(const std::string& path, std::string* error)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    char head[0x20] = {};
    const size_t n = f ? std::fread(head, 1, sizeof(head), f) : 0;
    if (f) std::fclose(f);
    if (!is_nro(std::string(head, n))) {
        if (error) *error = "not an NRO file";
        return false;
    }
    return true;
}

bool replace(const std::string& target, const std::string& fresh, std::string* error)
{
    const std::string old = target + ".old";
    std::remove(old.c_str());   // left by an earlier attempt
    // FAT has no atomic replace: move the old one aside first.
    const bool had_target = std::rename(target.c_str(), old.c_str()) == 0;
    if (!had_target && errno != ENOENT) {
        if (error) *error = "cannot move " + target + " aside";
        return false;
    }
    if (std::rename(fresh.c_str(), target.c_str()) != 0) {
        if (had_target) std::rename(old.c_str(), target.c_str());
        if (error) *error = "cannot put the new build in place";
        return false;
    }
    std::remove(old.c_str());
    return true;
}

}   // namespace dev_builds
