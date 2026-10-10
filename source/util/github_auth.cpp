// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/github_auth.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <borealis/extern/nlohmann/json.hpp>

#include "util/paths.hpp"

namespace github_auth
{

namespace
{
using nlohmann::json;

std::string str(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// GitHub tokens: letters, digits and '_' ("ghu_…"); nothing that could break a header.
bool token_chars(const std::string& t)
{
    return !t.empty() && t.size() <= 255 &&
           std::all_of(t.begin(), t.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '_'; });
}
}   // namespace

bool parse_device_code(const std::string& text, DeviceCode* out)
{
    try {
        const auto j = json::parse(text);
        if (!j.is_object()) return false;
        DeviceCode d;
        d.device_code = str(j, "device_code");
        d.user_code = str(j, "user_code");
        d.verification_uri = str(j, "verification_uri");
        if (j.contains("interval") && j["interval"].is_number_integer()) d.interval = j["interval"].get<int>();
        if (j.contains("expires_in") && j["expires_in"].is_number_integer()) d.expires_in = j["expires_in"].get<int>();
        if (d.device_code.empty() || d.user_code.empty() || d.verification_uri.compare(0, 8, "https://") != 0)
            return false;
        d.interval = std::min(std::max(d.interval, 1), 60);
        d.expires_in = std::min(std::max(d.expires_in, 60), 3600);
        *out = d;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

Poll parse_poll(const std::string& text, std::string* token, std::string* error)
{
    try {
        const auto j = json::parse(text);
        if (!j.is_object()) throw std::runtime_error("not an object");
        const std::string t = str(j, "access_token");
        if (!t.empty()) {
            if (!token_chars(t)) {
                if (error) *error = "unexpected token";
                return Poll::Failed;
            }
            *token = t;
            return Poll::Token;
        }
        const std::string e = str(j, "error");
        if (e == "authorization_pending") return Poll::Pending;
        if (e == "slow_down") return Poll::SlowDown;
        if (e == "expired_token") return Poll::Expired;
        if (e == "access_denied") return Poll::Denied;
        if (error) *error = e.empty() ? std::string("unexpected answer") : e;
        return Poll::Failed;
    } catch (const std::exception&) {
        if (error) *error = "unexpected answer";
        return Poll::Failed;
    }
}

std::string token_file()
{
    return paths::data_dir() + "/github_token";
}

std::string token()
{
    std::string t;
    if (!paths::read_file(token_file(), t)) return "";
    while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
    // The earlier OAuth app's token: refused the artifacts, signed in again.
    if (t.compare(0, 4, "gho_") == 0) return "";
    return token_chars(t) ? t : "";
}

bool save_token(const std::string& t, std::string* error)
{
    if (!token_chars(t)) {
        if (error) *error = "unexpected token";
        return false;
    }
    if (!paths::ensure_dir(paths::data_dir())) {
        if (error) *error = "cannot create " + paths::data_dir();
        return false;
    }
    return paths::atomic_write(token_file(), t + "\n", error);
}

void forget_token()
{
    std::remove(token_file().c_str());
    std::remove((token_file() + ".tmp").c_str());
}

std::vector<std::string> api_headers(const std::string& t)
{
    std::vector<std::string> h{ "Accept: application/vnd.github+json", "X-GitHub-Api-Version: 2022-11-28" };
    if (!t.empty()) h.push_back("Authorization: Bearer " + t);
    return h;
}

}   // namespace github_auth
