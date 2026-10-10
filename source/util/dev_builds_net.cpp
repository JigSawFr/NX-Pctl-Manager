// The network half of dev_builds.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app.hpp"
#include "util/dev_builds.hpp"
#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"
#include "util/zip_read.hpp"

namespace dev_builds
{

namespace
{
// A .nro is about 5 MB, its artifact a little less; anything much larger is not one.
constexpr uint64_t MAX_DOWNLOAD = 64 * 1024 * 1024;

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

// GET `path` of the API (or, simulated, the file `name` of the folder).
bool api_get(const std::string& path, const char* name, const std::string& token, std::string* body,
             std::string* error, size_t max)
{
#ifndef __SWITCH__
    if (const char* folder = sim()) {
        if (std::strcmp(folder, "offline") == 0 || !paths::read_file(std::string(folder) + "/" + name, *body)) {
            if (error) *error = "simulated: no connection";
            return false;
        }
        return true;
    }
#endif
    (void)name;
    return http::get(api_url() + path, body, error, max, 30, github_auth::api_headers(token));
}
}   // namespace

bool fetch(std::vector<Build>* out, bool* needs_login, std::string* error)
{
    out->clear();
    const std::string token = github_auth::token();
    *needs_login = token.empty();
    std::string body, err;
    bool reached = false;

    Build release;
    if (api_get("/releases/latest", "latest.json", token, &body, &err, 512 * 1024)) {
        reached = true;
        if (parse_release(body, &release)) out->push_back(release);
    }
    if (!token.empty()) {
        // One page of artifacts covers the last runs of main and of every
        // pull request; the open pull requests name the branches.
        std::vector<Artifact> artifacts;
        std::vector<Pull> pulls;
        const bool a = api_get("/actions/artifacts?name=playguard_release&per_page=100", "artifacts.json", token,
                               &body, &err, 4 * 1024 * 1024) &&
                       parse_artifacts(body, &artifacts);
        const bool p = api_get("/pulls?state=open&per_page=50", "pulls.json", token, &body, &err, 4 * 1024 * 1024) &&
                       parse_pulls(body, &pulls);
        reached |= a || p;
        for (Build& b : combine(artifacts, pulls)) out->push_back(std::move(b));
    }
    if (!out->empty()) return true;
    if (error) *error = reached ? std::string("no build found") : err;
    return false;
}

bool download(const Build& b, const std::string& path, std::string* error)
{
    if (b.size > MAX_DOWNLOAD) {
        if (error) *error = "too large for PlayGuard";
        return false;
    }
    // An artifact arrives as a zip next to the target, playguard.nro is taken out of it.
    const std::string file = b.artifact ? path + ".zip" : path;
    bool ok;
#ifndef __SWITCH__
    if (sim()) {
        std::string content;
        ok = b.url.compare(0, 8, "https://") == 0 && paths::read_file(b.url.substr(8), content) &&
             paths::atomic_write(file, content, error);
        if (!ok && error && error->empty()) *error = "simulated: cannot read " + b.url;
    } else
#endif
    if (b.artifact) {
        // The API answers with a short-lived signed link to the zip, on
        // another host: fetched without the token.
        std::string link;
        // The token only ever goes to the API itself, whatever the list says.
        const bool api = b.url.compare(0, 23, "https://api.github.com/") == 0;
        ok = http::redirect(b.url, &link, error, github_auth::api_headers(api ? github_auth::token() : ""));
        // A token without the Actions permission (an older sign-in) lists the
        // builds but is refused the download.
        if (!ok && error && (*error == "HTTP 401" || *error == "HTTP 403"))
            *error += ": GitHub refused the download, sign out of GitHub and sign in again";
        if (ok) ok = http::download(link, file, error, (size_t)b.size + 1);
    } else {
        ok = http::download(b.url, file, error, (size_t)b.size + 1);
    }
    if (ok) ok = verify_file(file, b.size, b.sha256, error);
    if (ok && b.artifact) ok = zip_read::extract(file, "playguard.nro", path, MAX_DOWNLOAD, error);
    if (ok) ok = verify_nro(path, error);
    if (b.artifact) std::remove(file.c_str());
    if (!ok) std::remove(path.c_str());
    return ok;
}

}   // namespace dev_builds
