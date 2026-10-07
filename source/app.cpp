// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "app.hpp"

#include "util/config.hpp"
#include "util/pctl_ops_c.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0-dev"
#endif

namespace app
{

static bool     s_ok          = false;
static uint32_t s_init_result = 0;
static bool     s_dev_session = false;

bool init()
{
    Result rc     = pctl_ops_init();
    s_init_result = (uint32_t)rc;
    s_ok          = R_SUCCEEDED(rc);
    pctl_ops_exit();   // every operation acquires its own session
    return s_ok;
}

void shutdown()
{
    pctl_ops_exit();
    s_ok = false;
}

bool        pctl_available()   { return s_ok; }
uint32_t    pctl_init_result() { return s_init_result; }
std::string version()          { return APP_VERSION; }
const char* repo_url()         { return "https://github.com/JigSawFr/PlayGuard"; }

bool in_focus()
{
#ifdef __SWITCH__
    return appletGetFocusState() == AppletFocusState_InFocus;
#else
    return true;
#endif
}

bool read_only()            { return core_read_only(); }
void set_read_only(bool on) { core_set_read_only(on); }

bool dev_mode() { return s_dev_session || config::get().dev_mode; }

bool set_dev_mode(bool on, bool persist)
{
    s_dev_session = on;
    if (persist || !on) {
        config::get().dev_mode = on;
        return config::save();
    }
    return true;
}

}   // namespace app
