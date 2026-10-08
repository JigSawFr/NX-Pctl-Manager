// HistoryActivity — what PlayGuard changed on the console (util/history.hpp),
// newest first. A on a change: its details and, when its previous value can
// be put back, the offer to (history_flow::open). The list is rebuilt after
// every change.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class HistoryActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/history.xml");

    void onContentAvailable() override;

  private:
    void rebuild();

    BRLS_BIND(brls::Label, note,  "hi_note");
    BRLS_BIND(brls::Box,   list,  "hi_list");
    BRLS_BIND(brls::Label, empty, "hi_empty");
};
