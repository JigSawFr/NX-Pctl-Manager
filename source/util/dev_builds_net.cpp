// The network half of dev_builds.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app.hpp"
#include "util/dev_builds.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace dev_builds
{

namespace
{
// A .nro is about 10 MB; anything much larger is not one.
constexpr size_t MAX_NRO = 64 * 1024 * 1024;

// "https://github.com/o/r" -> "https://api.github.com/repos/o/r"
std::string api_url()
{
    const std::string repo = app::repo_url();
    const std::string host = "https://github.com/";
    return "https://api.github.com/repos/" + repo.substr(host.size());
}

#ifndef __SWITCH__
const char* sim()
{
    const char* value = std::getenv("PLAYGUARD_SIM_DEV_BUILDS");
    return value && *value ? value : nullptr;
}
#endif
}   // namespace

bool fetch(std::vector<Build>* out, std::string* error)
{
    out->clear();
#ifndef __SWITCH__
    if (const char* file = sim()) {
        std::string json;
        if (std::strcmp(file, "offline") == 0 || !paths::read_file(file, json)) {
            if (error) *error = "simulated: no connection";
            return false;
        }
        if (!parse_releases(json, out)) {
            if (error) *error = "no build found";
            return false;
        }
        sort(out);
        return true;
    }
#endif
    // Three small requests (60 an hour are allowed without an account): the
    // latest release and "dev" by name, since the list is by creation date
    // and "dev" was created once, then the recent ones for the pull requests.
    std::string body, last_error;
    bool reached = false;
    const std::string api = api_url();
    for (const char* path : { "/releases/latest", "/releases/tags/dev" }) {
        if (http::get(api + path, &body, &last_error, 512 * 1024)) {
            reached = true;
            parse_release(body, out);
        }
    }
    if (http::get(api + "/releases?per_page=30", &body, &last_error, 4 * 1024 * 1024)) {
        reached = true;
        parse_releases(body, out);
    }
    sort(out);
    if (!out->empty()) return true;
    if (error) *error = reached ? std::string("no build found") : last_error;
    return false;
}

bool download(const Build& b, const std::string& path, std::string* error)
{
    if (b.size > MAX_NRO) {
        if (error) *error = "too large for an NRO";
        return false;
    }
    bool ok;
#ifndef __SWITCH__
    if (sim()) {
        // The simulated list points at local files.
        std::string content;
        ok = paths::read_file(b.url.substr(b.url.find("://") + 3), content) &&
             paths::atomic_write(path, content, error);
        if (!ok && error && error->empty()) *error = "simulated: cannot read " + b.url;
    } else
#endif
    ok = http::download(b.url, path, error, (size_t)b.size + 1);
    if (ok) ok = verify(path, b, error);
    if (!ok) std::remove(path.c_str());
    return ok;
}

}   // namespace dev_builds
