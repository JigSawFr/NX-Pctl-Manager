// The network half of github_auth.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>

#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/log_upload.hpp"

namespace github_auth
{

namespace
{
const http::Headers JSON{ "Accept: application/json" };

#ifndef __SWITCH__
const char* sim()
{
    const char* value = std::getenv("PLAYGUARD_SIM_GITHUB_LOGIN");
    return value && *value ? value : nullptr;
}
#endif
}   // namespace

bool start(DeviceCode* out, std::string* error)
{
    std::string body;
#ifndef __SWITCH__
    if (const char* s = sim()) {
        if (std::strcmp(s, "offline") == 0) {
            if (error) *error = "simulated: no connection";
            return false;
        }
        body = R"({"device_code": "sim", "user_code": "SIMU-1234", "verification_uri": "https://github.com/login/device",
                   "interval": 1, "expires_in": 900})";
    } else
#endif
    if (!http::post("https://github.com/login/device/code",
                    "client_id=" + std::string(CLIENT_ID), "application/x-www-form-urlencoded", &body,
                    error, nullptr, 8 * 1024, 30, JSON))
        return false;
    if (!parse_device_code(body, out)) {
        if (error) *error = "unexpected answer from GitHub";
        return false;
    }
    return true;
}

Poll poll(const DeviceCode& code, std::string* token, std::string* error)
{
    std::string body;
#ifndef __SWITCH__
    if (const char* s = sim()) {
        body = std::strcmp(s, "denied") == 0 ? R"({"error": "access_denied"})"
                                             : R"({"access_token": "ghu_simulated", "token_type": "bearer"})";
    } else
#endif
    if (!http::post("https://github.com/login/oauth/access_token",
                    "client_id=" + std::string(CLIENT_ID) + "&device_code=" + log_upload::url_encode(code.device_code) +
                        "&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code",
                    "application/x-www-form-urlencoded", &body, error, nullptr, 8 * 1024, 30, JSON))
        return Poll::Failed;
    return parse_poll(body, token, error);
}

}   // namespace github_auth
