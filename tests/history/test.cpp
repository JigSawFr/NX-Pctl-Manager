// Host tests for source/util/history.cpp: the change history on the SD card,
// newest first, trimmed to the newest 200, and what can be undone.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/history.hpp"
#include "util/paths.hpp"

static history::Entry limits(int from, int to)
{
    history::Entry e;
    e.when = "2026-10-08 18:30";
    e.kind = "limits";
    e.source = "uniform";
    e.before.assign(7, from);
    e.after.assign(7, to);
    return e;
}

static void test_round_trip()
{
    assert(history::load().empty());   // no file yet

    history::Entry level;
    level.when = "2026-10-08 18:31";
    level.kind = "level";
    level.before = { 3 };
    level.after = { 4 };
    history::Entry pin;
    pin.when = "2026-10-08 18:32";
    pin.kind = "pin";
    pin.source = "first_steps";

    assert(history::append(limits(120, 180)));
    assert(history::append(level));
    assert(history::append(pin));
    auto all = history::load();
    assert(all.size() == 3);
    assert(all[0].kind == "pin" && all[0].source == "first_steps" && all[0].before.empty());
    assert(all[1].kind == "level" && all[1].before == std::vector<int>{ 3 } && all[1].after == std::vector<int>{ 4 });
    assert(all[2].kind == "limits" && all[2].after.size() == 7 && all[2].after[3] == 180);

    assert(history::undoable(all[1]) && history::undoable(all[2]));
    assert(!history::undoable(all[0]));                 // an event, not a value
    history::Entry odd = all[2];
    odd.before.pop_back();
    assert(!history::undoable(odd));                    // 6 days is not a week
    odd = all[1];
    odd.kind = "something";
    assert(!history::undoable(odd));
}

static void test_trim_and_damage()
{
    for (int i = 0; i < 210; i++) assert(history::append(limits(i, i + 1)));
    auto all = history::load();
    assert(all.size() == history::MAX_ENTRIES);
    assert(all.front().after[0] == 210);                // the newest kept
    assert(all.back().after[0] == 210 - 199);           // the oldest kept

    // A damaged entry is skipped, a damaged file is an empty history.
    assert(paths::atomic_write(paths::history_file(),
        R"({"entries": [{"kind": "level", "before": [1], "after": [2]}, {"kind": 5}, "x",
                        {"kind": "limits", "before": "no"}]})"));
    all = history::load();
    assert(all.size() == 1 && all[0].kind == "level");
    assert(paths::atomic_write(paths::history_file(), "not json"));
    assert(history::load().empty());
    assert(history::append(limits(60, 90)) && history::load().size() == 1);
}

int main()
{
    char dir[] = "/tmp/playguard_history_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_round_trip();
    test_trim_and_damage();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("history round trip, order, trimming, damaged-file and undo assertions passed");
    return 0;
}
