// Host tests for source/util/launcher.cpp against a fake core/platform.h:
// which store is found on the SD card for each preference, and what reaches
// hbloader (an "sdmc:/" path, nothing when PlayGuard was not started by it).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "core/platform.h"
#include "util/launcher.hpp"
#include "util/paths.hpp"

static bool        g_hbloader = false;
static std::string g_next;
static int         g_calls = 0;

extern "C" bool platform_can_launch(void) { return g_hbloader; }
extern "C" bool platform_set_next_load(const char* path)
{
    g_calls++;
    if (!g_hbloader) return false;
    g_next = path;
    return true;
}

static void install(const std::string& sd_path)
{
    const std::string full = paths::sd_root() + sd_path;
    assert(std::system(("mkdir -p '" + full.substr(0, full.rfind('/')) + "'").c_str()) == 0);
    std::FILE* f = std::fopen(full.c_str(), "w");
    assert(f);
    std::fputs("NRO0", f);
    std::fclose(f);
}

int main()
{
    char dir[] = "/tmp/playguard_launcher_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::sd_root() is ./playguard_data/sd on the host

    using launcher::Store;
    // Nothing installed: no store, whatever the preference.
    for (const char* pref : { "auto", "sphaira", "appstore", "manual" }) assert(launcher::find(pref).store == Store::None);

    // A folder under a store's name is not the store.
    assert(std::system("mkdir -p playguard_data/sd/switch/appstore/appstore.nro") == 0);
    assert(launcher::find("appstore").store == Store::None);
    assert(std::system("rmdir playguard_data/sd/switch/appstore/appstore.nro") == 0);

    install("/switch/appstore/appstore.nro");
    assert(launcher::find("auto").store == Store::AppStore);
    assert(launcher::find("sphaira").store == Store::None);
    install("/switch/sphaira.nro");   // sphaira's second place
    assert(launcher::find("auto").path == "/switch/sphaira.nro");
    install("/switch/sphaira/sphaira.nro");
    const launcher::Target sphaira = launcher::find("auto");
    assert(sphaira.store == Store::Sphaira && sphaira.path == "/switch/sphaira/sphaira.nro");
    assert(launcher::find("appstore").path == "/switch/appstore/appstore.nro");
    assert(launcher::find("manual").store == Store::None);
    // Not started through hbloader: nothing to hand over.
    assert(!launcher::can_launch() && !launcher::launch(sphaira) && g_next.empty());
    g_hbloader = true;
    assert(launcher::can_launch());
    assert(launcher::launch(sphaira) && g_next == "sdmc:/switch/sphaira/sphaira.nro");
    const int calls = g_calls;
    assert(!launcher::launch(launcher::Target{}) && g_calls == calls);   // Store::None never reaches the loader
    assert(!launcher::launch(launcher::Target{ Store::Sphaira, "switch/x.nro" }) && g_calls == calls);
    // PlayGuard itself, after a development build replaced it.
    assert(launcher::launch_nro("/switch/playguard/playguard.nro") && g_next == "sdmc:/switch/playguard/playguard.nro");
    const int nro_calls = g_calls;
    assert(!launcher::launch_nro("") && !launcher::launch_nro("switch/playguard.nro") && g_calls == nro_calls);

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("launcher store search and hbloader hand-off assertions passed");
    return 0;
}
