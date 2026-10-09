// The network half of log_upload.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>

#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/log_upload.hpp"

namespace log_upload
{

namespace
{
Result to_bpaste(const std::string& text)
{
    Result r;
    std::string type, body;
    long status = 0;
    const std::string data = form(text, &type);
    if (!http::post("https://bpa.st/curl", data, type.c_str(), &body, &r.error, &status)) {
        r.error = "bpa.st: " + r.error;
        return r;
    }
    if (!parse_bpaste(body, &r.url, &r.removal)) {
        r.error = "unexpected answer from bpa.st (HTTP " + std::to_string(status) + ")";
        return r;
    }
    r.ok = true;
    return r;
}

Result to_gist(const std::string& text, const std::string& token, const std::string& version)
{
    Result r;
    std::string body;
    long status = 0;
    http::Headers headers = github_auth::api_headers(token);
    // The answer repeats the content: room for it, escaped.
    if (!http::post("https://api.github.com/gists", gist_body(text, version), "application/json", &body, &r.error,
                    &status, 3 * GIST_MAX_BYTES, 60, headers)) {
        // 401: the token was revoked; 403 / 404: it was given before PlayGuard
        // asked for the gist permission.
        r.relogin = status == 401 || status == 403 || status == 404;
        r.error = "GitHub: " + r.error;
        return r;
    }
    if (!parse_gist(body, &r.url)) {
        r.error = "unexpected answer from GitHub (HTTP " + std::to_string(status) + ")";
        return r;
    }
    r.ok = true;
    return r;
}
}   // namespace

Result upload(const std::string& text, Host host, const std::string& token, const std::string& version)
{
#ifndef __SWITCH__
    // Desktop simulation: "offline", or the link the server answers.
    if (const char* sim = std::getenv("PLAYGUARD_SIM_PASTE")) {
        Result r;
        if (std::strcmp(sim, "offline") == 0) {
            r.error = "simulated: no connection";
            return r;
        }
        r.ok = true;
        r.url = sim;
        return r;
    }
#endif
    if (host == Host::Gist) return to_gist(text, token, version);
    return to_bpaste(text);
}

}   // namespace log_upload
