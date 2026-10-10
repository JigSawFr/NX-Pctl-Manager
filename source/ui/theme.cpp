// ui — the theme colours and the console's system font (see ui.hpp).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#ifdef __SWITCH__
#include <switch.h>
#else
#include <cstdlib>
#endif

namespace ui
{

void register_theme_colors()
{
    // PlayGuard brand teal / amber, darkened on the light theme so every status
    // value keeps a contrast of at least 4.5:1 on the borealis backgrounds.
    auto& light = brls::Theme::getLightTheme();
    // The focus highlight, the click pulse and the active sidebar item in the
    // icon's teal (borealis' defaults are the Switch's cyan / blue), so the
    // app looks like its icon. Values keep borealis' blue: teal values would
    // read as the "ok" state.
    light.addColor("brls/highlight/color1", nvgRGB(0x14, 0xA3, 0x8A));
    light.addColor("brls/highlight/color2", nvgRGB(0x4A, 0xD6, 0xBA));
    light.addColor("brls/click_pulse", nvgRGBA(0x14, 0xA3, 0x8A, 38));
    light.addColor("brls/sidebar/active_item", nvgRGB(0x0F, 0x8A, 0x74));
    light.addColor("brand/ok", nvgRGB(0x0A, 0x6E, 0x5C));
    light.addColor("brand/warn", nvgRGB(0x8A, 0x52, 0x00));
    light.addColor("brand/bad", nvgRGB(0xB7, 0x1C, 0x1C));
    light.addColor("brand/gauge_track", nvgRGBA(0, 0, 0, 34));
    light.addColor("brand/note", nvgRGB(0x5C, 0x5C, 0x5C));
    auto& dark = brls::Theme::getDarkTheme();
    dark.addColor("brls/highlight/color1", nvgRGB(0x2E, 0xC4, 0xA6));
    dark.addColor("brls/highlight/color2", nvgRGB(0x9A, 0xF0, 0xDC));
    dark.addColor("brls/click_pulse", nvgRGBA(0x2E, 0xC4, 0xA6, 38));
    dark.addColor("brls/sidebar/active_item", nvgRGB(0x2E, 0xC4, 0xA6));
    // A clear green, not the logo teal: the dark theme's default value colour is
    // already teal, so "OK" would not stand out from plain values.
    dark.addColor("brand/ok", nvgRGB(0x7E, 0xD9, 0x57));
    dark.addColor("brand/warn", nvgRGB(0xFF, 0xB5, 0x47));
    dark.addColor("brand/bad", nvgRGB(0xFF, 0x7A, 0x7A));
    dark.addColor("brand/gauge_track", nvgRGBA(255, 255, 255, 46));
    dark.addColor("brand/note", nvgRGB(0xB8, 0xB8, 0xB8));
#ifndef __SWITCH__
    // Desktop simulation, PLAYGUARD_SIM_STILL_FOCUS=1 (the smoke test): the
    // glow borealis moves around the focus with the clock takes the stroke's
    // own colour, so a screenshot does not depend on when it was taken.
    if (std::getenv("PLAYGUARD_SIM_STILL_FOCUS")) {
        light.addColor("brls/highlight/color2", light["brls/highlight/color1"]);
        dark.addColor("brls/highlight/color2", dark["brls/highlight/color1"]);
    }
#endif
}

void use_latin_font()
{
#ifdef __SWITCH__
    // borealis (wiliwili's branch) draws every label with the Simplified
    // Chinese system font, the Latin one only filling its gaps. That font has
    // the pinyin letters é, è, ê… as full-width glyphs ("prot é g é e"), and a
    // full-width "·" and "…". Chinese keeps it: the han in its own style.
    const std::string locale = brls::Application::getLocale();
    if (locale.rfind("zh", 0) == 0) return;

    PlFontData standard;
    if (R_FAILED(plGetSharedFontByType(&standard, PlSharedFontType_Standard))) return;
    const int chinese = brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED);
    // Registered again under the name the default font is read from (once,
    // by the first label): the font data is the system's, not copied.
    if (!brls::Application::loadFontFromMemory(brls::FONT_CHINESE_SIMPLIFIED, standard.address, standard.size, false))
        return;
    const int latin = brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED);
    // The fallbacks the Chinese font had (fontstash looks one level deep).
    NVGcontext* vg = brls::Application::getNVGContext();
    for (int font : { chinese, brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED_EXT),
                      brls::Application::getFont(brls::FONT_CHINESE_TRADITIONAL),
                      brls::Application::getFont(brls::FONT_KOREAN_REGULAR),
                      brls::Application::getFont(brls::FONT_SWITCH_ICONS),
                      brls::Application::getFont(brls::FONT_MATERIAL_ICONS),
                      brls::Application::getFont(brls::FONT_EMOJI) })
        if (font != brls::FONT_INVALID) nvgAddFallbackFontId(vg, latin, font);
#endif
}

NVGcolor color_ok()      { return brls::Application::getTheme()["brand/ok"]; }
NVGcolor color_warn()    { return brls::Application::getTheme()["brand/warn"]; }
NVGcolor color_bad()     { return brls::Application::getTheme()["brand/bad"]; }
NVGcolor color_track()   { return brls::Application::getTheme()["brand/gauge_track"]; }
NVGcolor color_note()    { return brls::Application::getTheme()["brand/note"]; }
NVGcolor color_neutral() { return brls::Application::getTheme()["brls/list/listItem_value_color"]; }
NVGcolor color_text()    { return brls::Application::getTheme()["brls/text"]; }

}   // namespace ui
