// LockActivity — shown instead of the main screen when Security › Ask for the
// PIN is "To open PlayGuard": the system's PIN screen comes up at once; the
// right PIN opens the main screen, B quits. Also pushed again over the open
// screens when PlayGuard comes back after a while away (lock_again()): the
// right PIN then uncovers them.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LockActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/lock.xml");

    // `over`: pushed over the screens already open, which the PIN uncovers.
    explicit LockActivity(bool over = false) : over(over) {}

    void onContentAvailable() override;

    // Over whatever is open, unless a lock or recovery screen is already up.
    static void lock_again();

  private:
    bool over;

    void try_unlock();

    BRLS_BIND(brls::Button, enter, "lock_enter");
};
