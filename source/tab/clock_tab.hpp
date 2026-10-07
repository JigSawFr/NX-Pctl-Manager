// ClockTab — shows the system clocks and sets the network clock from a public
// NTP server (the play timer relies on the network clock).
// NTP sampling and the time:s write are adapted from anbingxi's fork.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "tab/tab_base.hpp"

class ClockTab : public TabBase
{
  public:
    ClockTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    struct Measurement
    {
        bool ok = false;
        uint64_t unix_seconds = 0;   // median server time at `at`
        std::chrono::steady_clock::time_point at;
        int64_t spread = 0;          // max - min between servers, seconds
        std::string server;
    };

    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    bool busy = false;
    std::string region_id;
    std::string server;
    Measurement last;

    void choose_region();
    void choose_server();
    void enter_custom();
    void set_server(const std::string& host, const std::string& region);
    void measure();
    void apply();

    BRLS_BIND(brls::DetailCell, user,     "ck_user");
    BRLS_BIND(brls::DetailCell, network,  "ck_network");
    BRLS_BIND(brls::DetailCell, accuracy, "ck_accuracy");
    BRLS_BIND(brls::DetailCell, autosync, "ck_auto");
    BRLS_BIND(brls::DetailCell, zone,     "ck_zone");
    BRLS_BIND(brls::Label,      why,      "ck_why");
    BRLS_BIND(brls::DetailCell, region,   "ck_region");
    BRLS_BIND(brls::DetailCell, server_cell, "ck_server");
    BRLS_BIND(brls::DetailCell, custom,   "ck_custom");
    BRLS_BIND(brls::DetailCell, measure_cell, "ck_measure");
    BRLS_BIND(brls::Label,      result,   "ck_result");
    BRLS_BIND(brls::DetailCell, apply_cell, "ck_apply");
    BRLS_BIND(brls::DetailCell, export_cell, "ck_export");
};
