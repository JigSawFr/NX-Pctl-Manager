// pt_flow — the "write-before-temporary-unlock" gate used by every play-timer
// write. If the timer is counting down, writing a new limit destabilises
// Atmosphère, so: ask the user → UnlockRestrictionTemporarily (1201) →
// verify IsRestrictionTemporaryUnlocked (1006) → let the caller write → lock
// again (automatically, or offered). The service layer re-checks the same state
// right before the write.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "util/pctl_ops_c.hpp"

namespace pt_flow
{

// Limits offered by the pickers, in minutes (0 = no play that day).
const std::vector<uint16_t>& quick_values();

// One dialog for the whole change. `body` asks the question ("Set 2 h for every
// day?"); when the timer is counting down the same dialog explains the
// temporary unlock and its button unlocks first. With an empty `body` and no
// unlock needed, `write` runs at once. `write(did_unlock)` only runs when it is
// safe to write; otherwise a toast has already explained why.
void confirm_write(const std::string& body, const std::string& confirm_label,
                   std::function<void(bool did_unlock)> write);

// After a write made through confirm_write: toast the result, then lock again
// if the write needed an unlock (at once when the "lock again automatically"
// preference is on, else by offering it), then `refresh`.
void finish_write(Result rc, bool did_unlock, const std::string& ok_text,
                  const std::string& error_prefix, std::function<void()> refresh);

// "Same limit every day" picker (quick values + Custom…), then confirm_write.
void choose_uniform_limit(const PtState& pt, std::function<void()> refresh);

}   // namespace pt_flow
