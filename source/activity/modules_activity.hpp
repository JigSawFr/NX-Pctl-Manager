// ModulesActivity — Tools › Optional modules: PlayGuard's two sysmodules,
// which it carries in its romfs and installs on the SD card
// (util/modules.hpp): the recovery module (acts once at boot when a RESCUE
// file asks it to) and the remote-link agent (keeps the link up while
// PlayGuard is closed). For each: its state, Install / Update, At boot,
// Start / Stop now (the agent), Remove. Every change asks for the PIN as a
// console change does: the recovery module can unlock the console.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/modules.hpp"

class ModulesActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/modules.xml");

    void onContentAvailable() override;

    // Where PlayGuard's copies are (the romfs; PLAYGUARD_SIM_BUNDLED on the
    // desktop build).
    static std::string bundle_dir();
    // "Installed", "Not installed" … for the cells that open this screen.
    static std::string summary(modules::Id id);

  private:
    struct Row
    {
        modules::Id       id;
        brls::DetailCell* status;
        brls::DetailCell* action;
        brls::BooleanCell* boot;
        brls::DetailCell* run;      // nullptr: the module only acts at boot
        brls::DetailCell* remove;
    };
    Row rows[2];

    void bind(Row& row);
    void refresh();
    void install(const Row& row);
    void uninstall(const Row& row);

    BRLS_BIND(brls::Label, intro, "md_intro");
};
