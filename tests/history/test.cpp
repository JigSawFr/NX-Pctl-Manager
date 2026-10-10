// Host tests for source/util/history.cpp: the change history on the SD card,
// newest first, trimmed to the newest 200, and what can be undone.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

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

static bool undoable_limits(int from, int to)
{
    return history::undoable(limits(from, to));
}

static void test_round_trip()
{
    CHECK(history::load().empty());   // no file yet

    history::Entry level;
    level.when = "2026-10-08 18:31";
    level.kind = "level";
    level.before = { 3 };
    level.after = { 4 };
    history::Entry pin;
    pin.when = "2026-10-08 18:32";
    pin.kind = "pin";
    pin.source = "first_steps";

    CHECK(history::append(limits(120, 180)));
    CHECK(history::append(level));
    CHECK(history::append(pin));
    auto all = history::load();
    CHECK(all.size() == 3);
    CHECK(all[0].kind == "pin" && all[0].source == "first_steps" && all[0].before.empty());
    CHECK(all[1].kind == "level" && all[1].before == std::vector<int>{ 3 } && all[1].after == std::vector<int>{ 4 });
    CHECK(all[2].kind == "limits" && all[2].after.size() == 7 && all[2].after[3] == 180);

    CHECK(history::undoable(all[1]) && history::undoable(all[2]));
    CHECK(!history::undoable(all[0]));                 // an event, not a value
    history::Entry odd = all[2];
    odd.before.pop_back();
    CHECK(!history::undoable(odd));                    // 6 days is not a week
    odd = all[1];
    odd.kind = "something";
    CHECK(!history::undoable(odd));
    // Limits changed outside PlayGuard (outside_watch): recorded with the
    // week before and after, never put back from the history.
    odd = all[2];
    odd.kind = "outside_limits";
    odd.before = odd.after;
    odd.before[0] = 60;
    CHECK(!history::undoable(odd));
}

static void test_value_ranges()
{
    // Each kind takes only what the console takes (the ranges of a restore).
    auto one = [](const char* kind, std::vector<int> before, std::vector<int> after) {
        history::Entry e;
        e.kind = kind;
        e.before = before;
        e.after = after;
        return history::undoable(e);
    };
    CHECK(one("level", { 0 }, { 4 }) && !one("level", { 5 }, { 4 }) && !one("level", { -1 }, { 4 }));
    CHECK(one("org", { 12 }, { 0 }) && !one("org", { 13 }, { 0 }));
    CHECK(one("vr", { 1 }, { 0 }) && !one("vr", { 2 }, { 0 }));
    CHECK(one("alarm", { 0 }, { 1 }) && !one("alarm", { 0 }, { -1 }));
    CHECK(one("custom", { 21, 1, 0 }, { 0, 0, 1 }) && !one("custom", { 22, 0, 0 }, { 0, 0, 0 }));
    CHECK(!one("custom", { 12, 2, 0 }, { 0, 0, 0 }));
    CHECK(undoable_limits(1440, 0xFFFF) && !undoable_limits(1441, 60) && !undoable_limits(60, -1));
    CHECK(!undoable_limits(0x10000, 60));

    // 64-bit values in the file are refused, not narrowed: 2^32 + 3 is not 3.
    CHECK(paths::atomic_write(paths::history_file(),
        R"({"entries": [{"kind": "level", "before": [4294967299], "after": [2]},
                        {"kind": "level", "before": [18446744073709551615], "after": [2]},
                        {"kind": "level", "before": [-4294967295], "after": [2]},
                        {"kind": "org", "before": [3], "after": [2]}]})"));
    auto all = history::load();
    CHECK(all.size() == 1 && all[0].kind == "org" && history::undoable(all[0]));
}

static void test_trim_and_damage()
{
    for (int i = 0; i < 210; i++) CHECK(history::append(limits(i, i + 1)));
    auto all = history::load();
    CHECK(all.size() == history::MAX_ENTRIES);
    CHECK(all.front().after[0] == 210);                // the newest kept
    CHECK(all.back().after[0] == 210 - 199);           // the oldest kept

    // A damaged entry is skipped, a damaged file is an empty history.
    CHECK(paths::atomic_write(paths::history_file(),
        R"({"entries": [{"kind": "level", "before": [1], "after": [2]}, {"kind": 5}, "x",
                        {"kind": "limits", "before": "no"}]})"));
    all = history::load();
    CHECK(all.size() == 1 && all[0].kind == "level");
    CHECK(paths::atomic_write(paths::history_file(), "not json"));
    CHECK(history::load().empty());
    // The next change does not overwrite it: it is kept as history.json.bad.
    CHECK(history::append(limits(60, 90)) && history::load().size() == 1);
    std::string kept;
    CHECK(paths::read_file(paths::history_file() + ".bad", kept) && kept == "not json");
    // A good file is appended to, and leaves the kept one alone.
    CHECK(history::append(limits(90, 120)) && history::load().size() == 2);
    CHECK(paths::read_file(paths::history_file() + ".bad", kept) && kept == "not json");
}

int main()
{
    char dir[] = "/tmp/playguard_history_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_round_trip();
    test_trim_and_damage();
    test_value_ranges();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
    return CHECK_DONE("history round trip, order, trimming, damaged-file, undo and value-range assertions passed");
}
