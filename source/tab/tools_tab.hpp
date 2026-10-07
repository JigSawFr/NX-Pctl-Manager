// ToolsTab — diagnostic export, preferences (language, theme, advanced) and About.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class ToolsTab : public TabBase
{
  public:
    ToolsTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    bool serial_revealed = false;   // until the tab is left (tabs are rebuilt when reopened)

    BRLS_BIND(brls::DetailCell,  export_cell, "tl_export");
    BRLS_BIND(brls::Label,       export_note, "tl_export_note");
    BRLS_BIND(brls::DetailCell,  language,    "tl_language");
    BRLS_BIND(brls::DetailCell,  theme,       "tl_theme");
    BRLS_BIND(brls::BooleanCell, auto_relock, "tl_auto_relock");
    BRLS_BIND(brls::BooleanCell, advanced,    "tl_advanced");
    BRLS_BIND(brls::DetailCell,  version,     "tl_version");
    BRLS_BIND(brls::DetailCell,  fw,          "tl_fw");
    BRLS_BIND(brls::DetailCell,  ams,         "tl_ams");
    BRLS_BIND(brls::DetailCell,  compat,      "tl_compat");
    BRLS_BIND(brls::DetailCell,  storage,     "tl_storage");
    BRLS_BIND(brls::DetailCell,  blank,       "tl_blank");
    BRLS_BIND(brls::DetailCell,  serial,      "tl_serial");
    BRLS_BIND(brls::Label,       serial_note, "tl_serial_note");
    BRLS_BIND(brls::DetailCell,  game_patches, "tl_patches");
    BRLS_BIND(brls::Label,       patches_note, "tl_patches_note");
    BRLS_BIND(brls::DetailCell,  mode,        "tl_mode");
    BRLS_BIND(brls::DetailCell,  data,        "tl_data");
    BRLS_BIND(brls::DetailCell,  license,     "tl_license");
    BRLS_BIND(brls::DetailCell,  source,      "tl_source");
    BRLS_BIND(brls::Label,       credits,     "tl_credits");
};
