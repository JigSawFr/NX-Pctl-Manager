// ClockTab — shows the system clocks and sets the network clock from a public
// NTP server (the play timer relies on the network clock). The measurement
// and the write are clock_flow's; this screen adds the server choice, the
// result text, the 2-minute countdown and a spinner while measuring.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "action/clock_flow.hpp"
#include "tab/tab_base.hpp"
#include "util/pctl_ops_c.hpp"

class ClockTab : public TabBase
{
  public:
    ClockTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    bool busy = false;
    bool autosync_off = false;   // "Synchronise Clock via Internet" read as off
    // The tab ticks every second; the clocks are read every SNAPSHOT_EVERY and
    // moved forward by the steady clock in between (a snapshot is ~16 IPCs).
    TimeSnapshot snap = {};
    std::chrono::steady_clock::time_point snap_at;
    bool snap_ok = false;
    std::string region_id;
    std::string server;
    clock_flow::Measurement last;

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
    BRLS_BIND(brls::Box,        progress, "ck_progress");
    BRLS_BIND(brls::Label,      progress_text, "ck_progress_text");
    BRLS_BIND(brls::Label,      result,   "ck_result");
    BRLS_BIND(brls::Label,      warning,  "ck_warning");
    BRLS_BIND(brls::DetailCell, apply_cell, "ck_apply");
};
