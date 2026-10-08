// PlayGuard — console services for the UI, with libnx (see platform.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "platform.h"

PlatformInput platform_numpad(const char *header, const char *guide, const char *initial, int max_len,
                              char *out, size_t out_size)
{
    if (!out || out_size == 0) return PLATFORM_INPUT_CANCELLED;
    out[0] = '\0';
    // The number pad with a ':' key, so "1:30" can be typed. borealis'
    // openForNumber reads the result with stoll and would stop at the colon.
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return PLATFORM_INPUT_NONE;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetLeftOptionalSymbolKey(&kbd, ":");
    swkbdConfigSetHeaderText(&kbd, header);
    swkbdConfigSetSubText(&kbd, guide);
    swkbdConfigSetStringLenMax(&kbd, (u32)max_len);
    swkbdConfigSetInitialText(&kbd, initial);
    swkbdConfigSetBlurBackground(&kbd, true);
    const Result rc = swkbdShow(&kbd, out, out_size);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) && out[0] ? PLATFORM_INPUT_OK : PLATFORM_INPUT_CANCELLED;
}

bool platform_in_focus(void)
{
    return appletGetFocusState() == AppletFocusState_InFocus;
}

bool platform_can_launch(void)
{
    return envHasNextLoad();
}

bool platform_set_next_load(const char *path)
{
    // hbloader passes the argument string as is; like its default
    // ("sdmc:/hbmenu.nro"), argv[0] is the full path of the homebrew.
    return path && envHasNextLoad() && R_SUCCEEDED(envSetNextLoad(path, path));
}

bool platform_region(int *region)
{
    SetRegion r;
    if (!region || R_FAILED(setGetRegionCode(&r))) return false;
    *region = (int)r;
    return true;
}

bool platform_random(void *buf, size_t size)
{
    if (!buf) return false;
    randomGet(buf, size);
    return true;
}
