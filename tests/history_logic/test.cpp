// Host tests for source/action/history_logic.cpp: what an undoable entry's
// dialog offers given the console's value now, and what the undo records.
#include <cassert>
#include <cstdio>
#include <vector>

#include "action/history_logic.hpp"

using history_logic::Undo;

static history::Entry entry(const char* kind, std::vector<int> before, std::vector<int> after)
{
    history::Entry e;
    e.when = "2026-10-08 18:30";
    e.kind = kind;
    e.before = std::move(before);
    e.after = std::move(after);
    return e;
}

static void test_action()
{
    const history::Entry level = entry("level", { 1 }, { 3 });
    assert(history_logic::undo_action(level, { 3 }, false, false) == Undo::Offer);
    assert(history_logic::undo_action(level, { 1 }, false, false) == Undo::AlreadyBack);
    // The console's value unread: still offered.
    assert(history_logic::undo_action(level, {}, false, false) == Undo::Offer);
    // Read-only: nothing to offer, but "already back" says more and comes first.
    assert(history_logic::undo_action(level, { 3 }, true, false) == Undo::ReadOnly);
    assert(history_logic::undo_action(level, {}, true, false) == Undo::ReadOnly);
    assert(history_logic::undo_action(level, { 1 }, true, false) == Undo::AlreadyBack);

    // The alarm only in advanced mode; read-only is said first.
    const history::Entry alarm = entry("alarm", { 0 }, { 1 });
    assert(history_logic::undo_action(alarm, { 1 }, false, false) == Undo::NeedsAdvanced);
    assert(history_logic::undo_action(alarm, { 1 }, false, true) == Undo::Offer);
    assert(history_logic::undo_action(alarm, { 1 }, true, false) == Undo::ReadOnly);
    assert(history_logic::undo_action(alarm, { 0 }, false, false) == Undo::AlreadyBack);

    const history::Entry limits = entry("limits", std::vector<int>(7, 120), std::vector<int>(7, 60));
    assert(history_logic::undo_action(limits, std::vector<int>(7, 60), false, false) == Undo::Offer);
    assert(history_logic::undo_action(limits, std::vector<int>(7, 120), false, false) == Undo::AlreadyBack);
    std::vector<int> other(7, 120);
    other[3] = 90;                                         // one day differs: not back
    assert(history_logic::undo_action(limits, other, false, false) == Undo::Offer);
}

static void test_changed_since()
{
    const history::Entry level = entry("level", { 1 }, { 3 });
    assert(!history_logic::changed_since(level, { 3 }));   // still the entry's value
    assert(history_logic::changed_since(level, { 2 }));    // changed again since
    assert(!history_logic::changed_since(level, {}));      // unread: nothing said
}

static void test_replaces()
{
    const history::Entry custom = entry("custom", { 12, 1, 0 }, { 16, 0, 0 });
    // The undo records the value it replaced: the console's when it was read.
    assert((history_logic::undo_replaces(custom, { 18, 1, 1 }) == std::vector<int>{ 18, 1, 1 }));
    assert((history_logic::undo_replaces(custom, {}) == std::vector<int>{ 16, 0, 0 }));
}

int main()
{
    test_action();
    test_changed_since();
    test_replaces();
    std::puts("history_logic undo offer, changed-since and recorded-value assertions passed");
    return 0;
}
