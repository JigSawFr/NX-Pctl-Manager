// PairingTab — link state of the Nintendo Switch Parental Controls phone app.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class PairingTab : public TabBase
{
  public:
    PairingTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    BRLS_BIND(brls::DetailCell, active,  "pr_active");
    BRLS_BIND(brls::DetailCell, updated, "pr_updated");
    BRLS_BIND(brls::DetailCell, unlink,  "pr_unlink");
    BRLS_BIND(brls::Label,      note,    "pr_note");
};
