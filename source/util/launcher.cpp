// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/launcher.hpp"

#include <sys/stat.h>

#include "core/platform.h"
#include "util/paths.hpp"

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
    return platform_can_launch();
}

bool launch(const Target& target)
{
    if (target.store == Store::None || target.path.empty() || target.path[0] != '/') return false;
    return platform_set_next_load(("sdmc:" + target.path).c_str());
}

}   // namespace launcher
