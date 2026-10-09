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
bool is_hex(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isxdigit((unsigned char)c) != 0; });
}

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string str(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// The asset fields every build needs; false when one is missing.
bool asset_fields(const nlohmann::json& a, Build* b)
{
    const auto size = a.find("size");
    b->url = str(a, "browser_download_url");
    b->date = str(a, "updated_at");
    b->sha256 = digest_hex(str(a, "digest"));
    if (size == a.end() || !size->is_number_unsigned() || b->url.compare(0, 8, "https://") != 0) return false;
    b->size = size->get<uint64_t>();
    return b->size > 0;
}

// "pr-38" -> 38; 0 for anything else.
int pr_number(const std::string& tag)
{
    if (tag.size() < 4 || tag.size() > 9 || tag.compare(0, 3, "pr-") != 0) return 0;
    const std::string n = tag.substr(3);
    if (!std::all_of(n.begin(), n.end(), ::isdigit) || n[0] == '0') return 0;
    return std::stoi(n);
}

bool parse_object(const nlohmann::json& r, std::vector<Build>* out)
{
    if (!r.is_object() || !r.contains("assets") || !r["assets"].is_array()) return false;
    const std::string tag = str(r, "tag_name");
    const bool pre = r.value("prerelease", false);
    const bool draft = r.value("draft", false);
    if (draft) return false;
    const int pr = pr_number(tag);
    std::vector<Build> found;
    for (const auto& a : r["assets"]) {
        if (!a.is_object()) continue;
        const std::string name = str(a, "name");
        Build b;
        if (!pre && name == "playguard.nro") {
            b.kind = Kind::Release;
            b.version = tag.size() > 1 && tag[0] == 'v' ? tag.substr(1) : tag;
        } else if (pre && (tag == "dev" || pr) && !(b.commit = commit_of(name)).empty()) {
            b.kind = tag == "dev" ? Kind::Main : Kind::PullRequest;
            b.pr = pr;
            if (pr) b.title = str(r, "name");
        } else {
            continue;
        }
        if (asset_fields(a, &b)) found.push_back(b);
    }
    if (found.empty()) return false;
    if (pr) {
        // Only the newest build of a pull request.
        auto newest = std::max_element(found.begin(), found.end(),
                                       [](const Build& x, const Build& y) { return x.date < y.date; });
        found = { *newest };
    }
    out->insert(out->end(), found.begin(), found.end());
    return true;
}
}   // namespace

std::string commit_of(const std::string& asset_name)
{
    static const std::string PREFIX = "playguard-", SUFFIX = ".nro";
    if (asset_name.size() != PREFIX.size() + 7 + SUFFIX.size()) return "";
    if (asset_name.compare(0, PREFIX.size(), PREFIX) != 0) return "";
    if (asset_name.compare(asset_name.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX) != 0) return "";
    const std::string commit = asset_name.substr(PREFIX.size(), 7);
    return is_hex(commit) ? lower(commit) : "";
}

bool parse_release(const std::string& json, std::vector<Build>* out)
{
    try {
        return parse_object(nlohmann::json::parse(json), out);
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_releases(const std::string& json, std::vector<Build>* out)
{
    try {
        const auto j = nlohmann::json::parse(json);
        if (!j.is_array()) return false;
        bool any = false;
        for (const auto& r : j) any |= parse_object(r, out);
        return any;
    } catch (const std::exception&) {
        return false;
    }
}

void sort(std::vector<Build>* builds)
{
    auto rank = [](Kind k) { return k == Kind::Release ? 0 : k == Kind::Main ? 1 : 2; };
    std::stable_sort(builds->begin(), builds->end(), [&rank](const Build& a, const Build& b) {
        if (rank(a.kind) != rank(b.kind)) return rank(a.kind) < rank(b.kind);
        if (a.kind == Kind::PullRequest && a.pr != b.pr) return a.pr > b.pr;
        return a.date > b.date;
    });
    std::set<std::string> seen;
    builds->erase(std::remove_if(builds->begin(), builds->end(),
                                 [&seen](const Build& b) {
                                     const std::string key = std::to_string((int)b.kind) + ":" +
                                                             std::to_string(b.pr) + ":" + b.commit + b.version;
                                     return !seen.insert(key).second;
                                 }),
                  builds->end());
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

bool verify(const std::string& path, const Build& b, std::string* error)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "cannot read the download";
        return false;
    }
    Sha256 hash;
    std::string head;
    uint64_t size = 0;
    char buf[16 * 1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (head.size() < 0x20) head.append(buf, std::min(n, (size_t)0x20 - head.size()));
        hash.update(buf, n);
        size += n;
    }
    const bool read_error = std::ferror(f) != 0;
    std::fclose(f);
    if (read_error) {
        if (error) *error = "cannot read the download";
        return false;
    }
    if (size != b.size) {
        if (error) *error = "size " + std::to_string(size) + " instead of " + std::to_string(b.size);
        return false;
    }
    if (!b.sha256.empty() && hash.hex() != b.sha256) {
        if (error) *error = "SHA-256 does not match";
        return false;
    }
    if (!is_nro(head)) {
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
