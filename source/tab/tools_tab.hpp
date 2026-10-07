// ToolsTab — diagnostic export, settings backup / restore, preferences
// (language, theme, advanced, update store), About with the update check, and
// the developer tools (shown after seven presses on Version).
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
    int        version_presses     = 0;
    brls::Time last_version_press  = 0;

    void count_version_press();

    BRLS_BIND(brls::DetailCell,  export_cell, "tl_export");
    BRLS_BIND(brls::Label,       export_note, "tl_export_note");
    BRLS_BIND(brls::DetailCell,  backup_save, "tl_backup_save");
    BRLS_BIND(brls::DetailCell,  backup_restore, "tl_backup_restore");
    BRLS_BIND(brls::Label,       backup_note, "tl_backup_note");
    BRLS_BIND(brls::DetailCell,  language,    "tl_language");
    BRLS_BIND(brls::DetailCell,  theme,       "tl_theme");
    BRLS_BIND(brls::BooleanCell, auto_relock, "tl_auto_relock");
    BRLS_BIND(brls::BooleanCell, extra_auto,  "tl_extra_auto");
    BRLS_BIND(brls::DetailCell,  start_tab,   "tl_start_tab");
    BRLS_BIND(brls::DetailCell,  extra_amounts, "tl_extra_amounts");
    BRLS_BIND(brls::BooleanCell, clock_check, "tl_clock_check");
    BRLS_BIND(brls::BooleanCell, update_daily, "tl_update_daily");
    BRLS_BIND(brls::DetailCell,  backup_keep, "tl_backup_keep");
    BRLS_BIND(brls::BooleanCell, advanced,    "tl_advanced");
    BRLS_BIND(brls::DetailCell,  update_via,  "tl_update_via");
    BRLS_BIND(brls::DetailCell,  version,     "tl_version");
    BRLS_BIND(brls::DetailCell,  update_cell, "tl_update");
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
    BRLS_BIND(brls::Header,      dev_header,  "tl_dev_header");
    BRLS_BIND(brls::BooleanCell, dev_mode,    "tl_dev_mode");
    BRLS_BIND(brls::BooleanCell, dev_read_only, "tl_dev_read_only");
    BRLS_BIND(brls::DetailCell,  dev_report,  "tl_dev_report");
    BRLS_BIND(brls::DetailCell,  dev_gate,    "tl_dev_gate");
    BRLS_BIND(brls::DetailCell,  dev_forget,  "tl_dev_forget");
};
