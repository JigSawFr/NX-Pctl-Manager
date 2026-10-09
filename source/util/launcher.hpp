// launcher — hands over to a homebrew store to update PlayGuard. It sets the
// homebrew that hbloader starts once this app exits (envSetNextLoad), so it
// works whichever menu (hbmenu, sphaira) PlayGuard was started from.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace launcher
{

enum class Store
{
    None,
    Sphaira,
    AppStore,   // Homebrew App Store (hb-appstore)
};

struct Target
{
    Store       store = Store::None;
    std::string path;   // from the SD card root: "/switch/sphaira/sphaira.nro"
};

// `preference`: "auto" (sphaira, else the App Store), "sphaira", "appstore"
// or "manual" (never a store). Store::None when nothing usable is installed.
Target find(const std::string& preference);

// The loader can start another homebrew after this one (false on desktop and
// when PlayGuard was not started through hbloader).
bool can_launch();

// Sets `target` as the next homebrew; the caller then quits the app.
bool launch(const Target& target);

// The same for any .nro, from the SD card root ("/switch/playguard/playguard.nro"):
// PlayGuard itself after it was replaced (dev_build_flow).
bool launch_nro(const std::string& path);

}   // namespace launcher
