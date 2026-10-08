// LockActivity — shown instead of the main screen when Security › Ask for the
// PIN is "To open PlayGuard": the system's PIN screen comes up at once; the
// right PIN opens the main screen, B quits.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LockActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/lock.xml");

    void onContentAvailable() override;

  private:
    void try_unlock();

    BRLS_BIND(brls::Button, enter, "lock_enter");
};
