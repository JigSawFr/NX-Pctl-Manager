// DiagnosticActivity — developer mode: the diagnostic report (firmware, clocks,
// every pctl query) read on the console itself; Y saves it like Tools › Export,
// X sends it online like Tools › Send a report online.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class DiagnosticActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/diagnostic.xml");

    void onContentAvailable() override;

  private:
    std::string report;

    BRLS_BIND(brls::Box, list, "dg_list");
};
