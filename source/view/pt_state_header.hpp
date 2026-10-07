// PtStateHeader — the play-timer state block (enabled, limit reached,
// temporary unlock, remaining today, configured limit) shown at the top of the
// Play timer tab and the per-day editor.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

class PtStateHeader : public brls::Box
{
  public:
    PtStateHeader();

    void refresh();                    // queries the service
    void show(const PtState& pt);      // renders an existing snapshot

    static brls::View* create();
    static std::string configured_text(const PtState& pt);

  private:
    BRLS_BIND(brls::Label, enabled_value,    "pt_enabled_value");
    BRLS_BIND(brls::Label, restricted_value, "pt_restricted_value");
    BRLS_BIND(brls::Label, temporary_value,  "pt_temporary_value");
    BRLS_BIND(brls::Label, remaining_value,  "pt_remaining_value");
    BRLS_BIND(brls::Label, configured_value, "pt_configured_value");
};
