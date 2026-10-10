// GameCheckActivity — Tools › Check the installed games: the games the HOME
// menu cannot start or draw (core/gamecheck.h), each with what the HOME menu
// shows. A on one: the cause, what to do and, when it helps, the offer to
// remove it (save data kept). The games are read off the main thread when
// the screen opens and again after a removal.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <memory>

#include "util/pctl_ops_c.hpp"

class GameCheckActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/game_check.xml");

    void onContentAvailable() override;

  private:
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    bool scanning = false;
    bool focused  = false;   // the first game got the focus once

    void scan();
    void show(const GameCheck& check);
    void open(const GameFacts& game, GameIssue issue, u32 current_hos);

    BRLS_BIND(brls::Label,  note,   "gc_note");
    BRLS_BIND(brls::Label,  status, "gc_status");
    BRLS_BIND(brls::Header, header, "gc_header");
    BRLS_BIND(brls::Box,    list,   "gc_list");
    BRLS_BIND(brls::Label,  hint,   "gc_hint");
};
