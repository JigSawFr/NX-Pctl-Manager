// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/launcher.hpp"

#include <sys/stat.h>

#include "util/paths.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace launcher
{

namespace
{
// Where each store installs itself. sphaira looks for its own copy in the
// first two (sphaira/source/app.cpp); hb-appstore unzips to /switch/appstore/.
const char* const SPHAIRA[]  = { "/switch/sphaira/sphaira.nro", "/switch/sphaira.nro" };
const char* const APPSTORE[] = { "/switch/appstore/appstore.nro" };

bool exists(const std::string& sd_path)
{
    std::string root = paths::sd_root();
    if (!root.empty() && root.back() == '/') root.pop_back();
    struct stat st;
    return stat((root + sd_path).c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

template <size_t N>
Target first_existing(Store store, const char* const (&candidates)[N])
{
    for (const char* p : candidates)
        if (exists(p)) return Target{ store, p };
    return Target{};
}
}   // namespace

Target find(const std::string& preference)
{
    if (preference == "manual") return Target{};
    if (preference == "sphaira") return first_existing(Store::Sphaira, SPHAIRA);
    if (preference == "appstore") return first_existing(Store::AppStore, APPSTORE);
    Target t = first_existing(Store::Sphaira, SPHAIRA);
    return t.store != Store::None ? t : first_existing(Store::AppStore, APPSTORE);
}

bool can_launch()
{
#ifdef __SWITCH__
    return envHasNextLoad();
#else
    return false;
#endif
}

bool launch(const Target& target)
{
#ifdef __SWITCH__
    if (target.store == Store::None || !envHasNextLoad()) return false;
    // hbloader passes the argument string as is; like its default
    // ("sdmc:/hbmenu.nro"), argv[0] is the full path of the homebrew.
    const std::string path = "sdmc:" + target.path;
    return R_SUCCEEDED(envSetNextLoad(path.c_str(), path.c_str()));
#else
    (void)target;
    return false;
#endif
}

}   // namespace launcher
