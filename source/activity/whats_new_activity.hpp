// WhatsNewActivity — "What's new in PlayGuard X.Y.Z": shown once at start-up
// after an update (action/support_flow), the running version's notes, a
// Close cell, then the funding QR codes. B closes it too.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class WhatsNewActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/whats_new.xml");

    void onContentAvailable() override;

  private:
    BRLS_BIND(brls::Header,         header,       "wn_header");
    BRLS_BIND(brls::Box,            notes,        "wn_notes");
    BRLS_BIND(brls::DetailCell,     close,        "wn_close");
    BRLS_BIND(brls::Label,          support_note, "wn_support_note");
    BRLS_BIND(brls::Box,            funding_box,  "wn_funding");
};
