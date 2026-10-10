// SyncActivity — Preferences › Remote access: the remote link's settings
// (sync.conf: the MQTT broker, the console's name, what orders from Home
// Assistant may do), its live status, "Sync now" and its log. Every change
// of a setting goes through the PIN check of a console change (Security ›
// Ask for the PIN): the link can change the console.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

#include "sync/sync_conf.h"

class SyncActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/sync.xml");

    void onContentAvailable() override;

  private:
    SyncConf conf{};
    brls::RepeatingTimer status_timer;
    std::string status_line;   // what the status cell says

    void refresh();
    void refresh_status();
    // Saves sync.conf after the PIN check, applies it, refreshes. False when
    // refused or not saved (the settings on screen go back).
    bool commit(SyncConf next);

    BRLS_BIND(brls::Label,       intro,     "sy_intro");
    BRLS_BIND(brls::BooleanCell, enabled,   "sy_enabled");
    BRLS_BIND(brls::DetailCell,  host,      "sy_host");
    BRLS_BIND(brls::DetailCell,  port,      "sy_port");
    BRLS_BIND(brls::BooleanCell, tls,       "sy_tls");
    BRLS_BIND(brls::DetailCell,  mqtt_version, "sy_mqtt_version");
    BRLS_BIND(brls::DetailCell,  user,      "sy_user");
    BRLS_BIND(brls::DetailCell,  password,  "sy_password");
    BRLS_BIND(brls::DetailCell,  name,      "sy_name");
    BRLS_BIND(brls::DetailCell,  policy,    "sy_policy");
    BRLS_BIND(brls::BooleanCell, timer,     "sy_timer");
    BRLS_BIND(brls::BooleanCell, discovery, "sy_discovery");
    BRLS_BIND(brls::BooleanCell, report,    "sy_report");
    BRLS_BIND(brls::DetailCell,  status,    "sy_status");
    BRLS_BIND(brls::DetailCell,  sync_now,  "sy_sync_now");
    BRLS_BIND(brls::DetailCell,  log,       "sy_log");
    BRLS_BIND(brls::DetailCell,  id,        "sy_id");
    BRLS_BIND(brls::Label,       note,      "sy_note");
};
