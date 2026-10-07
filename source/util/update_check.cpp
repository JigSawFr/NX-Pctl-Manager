// The network half of update.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>

#include "app.hpp"
#include "util/http.hpp"
#include "util/update.hpp"

namespace update
{

std::string compat_url()
{
    return std::string(app::repo_url()) + "/releases/latest/download/compat.json";
}

Result check(uint32_t console_fw)
{
    Result r;
    std::string body;
#ifndef __SWITCH__
    // Desktop simulation: "1.1.0:24.0.0" (latest version : firmware it supports) or "offline".
    if (const char* sim = std::getenv("PLAYGUARD_SIM_LATEST")) {
        const char* colon = std::strchr(sim, ':');
        if (!colon) {
            r.error = "simulated: no connection";
            return r;
        }
        body = "{\"schema\": 1, \"version\": \"" + std::string(sim, colon) + "\", \"fw_tested_max\": \"" +
               std::string(colon + 1) + "\"}";
    } else
#endif
    if (!http::get(compat_url(), &body, &r.error)) {
        return r;
    }
    if (!parse_compat(body, &r.latest)) {
        r.error = "unexpected compat.json";
        return r;
    }
    r.verdict = decide(console_fw, app::version(), r.latest);
    return r;
}

}   // namespace update
