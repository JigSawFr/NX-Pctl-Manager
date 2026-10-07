// InitErrorActivity — root Activity when the pctl service cannot be opened
// (typically: no CFW). One-shot screen; B exits the app.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class InitErrorActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/init_error.xml");

    void onContentAvailable() override;

  private:
    BRLS_BIND(brls::Label, error_code, "error_code");
    BRLS_BIND(brls::Label, error_fw,   "error_fw");
};
