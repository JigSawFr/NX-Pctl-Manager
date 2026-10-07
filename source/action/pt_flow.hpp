// pt_flow — the "write-before-temporary-unlock" gate used by every play-timer
// write. If the timer is counting down, writing a new limit destabilises
// Atmosphère, so: ask the user → UnlockRestrictionTemporarily (1201) →
// verify IsRestrictionTemporaryUnlocked (1006) → let the caller write. The
// service layer re-checks the same state right before the write.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>

namespace pt_flow
{

// on_ready(true, did_unlock) when it is safe to write; on_ready(false, false)
// when the user declined or the state could not be verified (a toast has
// already explained why). The caller must not write in that case.
void ready_to_write(std::function<void(bool ok, bool did_unlock)> on_ready);

// After a write that needed an unlock: offer to re-lock immediately (1007).
void offer_relock(std::function<void()> after = nullptr);

}   // namespace pt_flow
