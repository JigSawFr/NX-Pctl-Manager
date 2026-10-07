// FirmwareGateActivity — shown over the main screen when the firmware is newer
// than the checked range (see action/fw_gate.hpp): says so, looks for a
// release that supports it, and offers to update, to continue read-only (with
// the developer tools or not) or to continue at the user's own risk.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <memory>

#include "action/fw_gate.hpp"

class FirmwareGateActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/firmware_gate.xml");

    void onContentAvailable() override;

  private:
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);   // guards the async update check
    bool chosen = false;

    void choose(fw_gate::Choice choice);

    BRLS_BIND(brls::Label,       headline,   "fg_headline");
    BRLS_BIND(brls::Label,       body,       "fg_body");
    BRLS_BIND(brls::Label,       status,     "fg_status");
    BRLS_BIND(brls::DetailCell,  update,     "fg_update");
    BRLS_BIND(brls::DetailCell,  read_only,  "fg_read_only");
    BRLS_BIND(brls::DetailCell,  probe,      "fg_probe");
    BRLS_BIND(brls::DetailCell,  risk,       "fg_risk");
    BRLS_BIND(brls::BooleanCell, remember,   "fg_remember");
    BRLS_BIND(brls::Label,       note,       "fg_note");
    BRLS_BIND(brls::DetailCell,  quit,       "fg_quit");
};
