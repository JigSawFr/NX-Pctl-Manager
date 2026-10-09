// The network half of log_upload.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>

#include "util/http.hpp"
#include "util/log_upload.hpp"

namespace log_upload
{

Result upload(const std::string& text)
{
    Result r;
    std::string body;
    long status = 0;
#ifndef __SWITCH__
    // Desktop simulation: "offline", or the link the server answers.
    if (const char* sim = std::getenv("PLAYGUARD_SIM_PASTE")) {
        if (std::strcmp(sim, "offline") == 0) {
            r.error = "simulated: no connection";
            return r;
        }
        body = sim;
        status = 201;
    } else
#endif
    {
        std::string type;
        const std::string data = form(text, &type);
        if (!http::post("https://dpaste.org/api/", data, type.c_str(), &body, &r.error, &status)) {
            r.error = "dpaste.org: " + r.error;
            return r;
        }
    }
    if (!parse_reply(body, &r.url)) {
        r.error = "unexpected answer from dpaste.org (HTTP " + std::to_string(status) + ")";
        return r;
    }
    r.ok = true;
    return r;
}

}   // namespace log_upload
