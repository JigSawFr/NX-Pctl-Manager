// data_notice — tells the parent, once, that PlayGuard found one of its own
// files damaged: config.json (the settings were reset, the file kept as
// config.json.bad), history.json (a new history started, the old one kept as
// history.json.bad), or a recovery report it could not read. One dialog for
// all of them on the main screen at start-up; a reset of the settings is also
// recorded in the change history.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace data_notice
{

// rescue::take() found a report it cannot read (it is removed all the same).
void rescue_unreadable();
// history::append() put a damaged history.json aside: told at start-up, or
// at once when that is past.
void history_put_aside();

// MainActivity, once per start: puts a damaged history aside now
// (history::check), then shows what was found, if anything.
void at_start();

}   // namespace data_notice
