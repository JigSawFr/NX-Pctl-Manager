// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/history_logic.hpp"

namespace history_logic
{

Undo undo_action(const history::Entry& e, const std::vector<int>& now, bool read_only, bool advanced)
{
    if (!now.empty() && now == e.before) return Undo::AlreadyBack;
    if (read_only) return Undo::ReadOnly;
    if (e.kind == "alarm" && !advanced) return Undo::NeedsAdvanced;
    return Undo::Offer;
}

bool changed_since(const history::Entry& e, const std::vector<int>& now)
{
    return !now.empty() && now != e.after;
}

std::vector<int> undo_replaces(const history::Entry& e, const std::vector<int>& now)
{
    return now.empty() ? e.after : now;
}

}   // namespace history_logic
