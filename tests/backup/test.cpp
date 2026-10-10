// Host tests for source/util/backup.cpp: the JSON format, the validation that
// keeps a damaged file away from the console, and save / list / load.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "util/backup.hpp"
#include "util/paths.hpp"

static backup::Snapshot full()
{
    backup::Snapshot s;
    s.created  = "2026-10-07 18:30";
    s.firmware = "23.0.1";
    s.level_ok = true;
    s.level    = 1;
    s.custom_ok       = true;
    s.rating_age      = 12;
    s.sns_restricted  = true;
    s.comm_restricted = false;
    s.vr_ok         = true;
    s.vr_restricted = true;
    s.days_ok = true;
    s.days    = { 180, 120, 120, 0xFFFF, 120, 0, 1440 };
    s.alarm_ok       = true;
    s.alarm_disabled = true;
    s.rating_org_ok  = true;
    s.rating_org     = 6;
    s.raw_block      = std::string(4, '0') + "0101" + std::string(128, 'A');
    return s;
}

static bool same(const backup::Snapshot& a, const backup::Snapshot& b)
{
    return a.created == b.created && a.firmware == b.firmware && a.level_ok == b.level_ok &&
           a.level == b.level && a.custom_ok == b.custom_ok && a.rating_age == b.rating_age &&
           a.sns_restricted == b.sns_restricted && a.comm_restricted == b.comm_restricted &&
           a.vr_ok == b.vr_ok && a.vr_restricted == b.vr_restricted && a.days_ok == b.days_ok &&
           a.days == b.days && a.alarm_ok == b.alarm_ok && a.alarm_disabled == b.alarm_disabled &&
           a.rating_org_ok == b.rating_org_ok && a.rating_org == b.rating_org && a.raw_block == b.raw_block;
}

static bool parses(const std::string& text)
{
    backup::Snapshot s;
    return backup::from_json(text, s);
}

// A valid file with one fragment replaced, to check each value is validated.
static std::string with(const std::string& from, const std::string& to)
{
    std::string text = backup::to_json(full());
    const size_t at = text.find(from);
    CHECK(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

static void test_round_trip()
{
    backup::Snapshot in = full(), out;
    const std::string text = backup::to_json(in);
    CHECK(text.find("\"days_from_sunday\"") != std::string::npos);
    CHECK(text.find("null") != std::string::npos);   // no limit on Wednesday
    CHECK(text.find("pin") == std::string::npos);
    CHECK(backup::from_json(text, out) && same(in, out));

    // Values that could not be read are left out, and stay unset when read back.
    in = backup::Snapshot();
    in.vr_ok = true;
    in.vr_restricted = false;
    const std::string partial = backup::to_json(in);
    CHECK(partial.find("\"level\"") == std::string::npos && partial.find("play_timer") == std::string::npos);
    out = full();
    CHECK(backup::from_json(partial, out));
    CHECK(out.vr_ok && !out.vr_restricted && !out.level_ok && !out.custom_ok && !out.days_ok);

    // Nothing at all to restore: refused.
    CHECK(!parses(backup::to_json(backup::Snapshot())));
}

static void test_validation()
{
    CHECK(!parses(""));
    CHECK(!parses("not json"));
    CHECK(!parses("[]"));
    CHECK(!parses("{\"restrictions\":{\"level\":3}}"));                 // no format
    CHECK(!parses("{\"format\":2,\"restrictions\":{\"level\":3}}"));    // unknown format
    CHECK(parses("{\"format\":1,\"restrictions\":{\"level\":3}}"));

    CHECK(!parses(with("\"level\": 1", "\"level\": 5")));
    CHECK(!parses(with("\"level\": 1", "\"level\": -1")));
    CHECK(!parses(with("\"level\": 1", "\"level\": 1.5")));
    CHECK(!parses(with("\"level\": 1", "\"level\": \"1\"")));
    CHECK(!parses(with("\"rating_age\": 12", "\"rating_age\": 22")));
    CHECK(!parses(with("\"rating_age\": 12,", "")));                    // custom settings incomplete
    CHECK(!parses(with("\"sns_post_restricted\": true", "\"sns_post_restricted\": 1")));
    CHECK(!parses(with("\"vr_restricted\": true", "\"vr_restricted\": \"yes\"")));
    CHECK(!parses(with("180", "1441")));
    CHECK(!parses(with("180", "-5")));
    CHECK(!parses(with("180,", "")));                                   // six days
    CHECK(!parses(with("\"days_from_sunday\"", "\"days\"")));
    CHECK(parses(with("180", "0")));

    // The fields kept for the record are validated too.
    CHECK(!parses(with("\"alarm_disabled\": true", "\"alarm_disabled\": 1")));
    CHECK(!parses(with("\"rating_organization\": 6", "\"rating_organization\": 13")));
    CHECK(!parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"zz00")));
    CHECK(!parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"00")));       // 134 digits
    CHECK(parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"abcd")));      // either case

    // A backup from before these fields still reads.
    backup::Snapshot old_one = full();
    old_one.alarm_ok = old_one.rating_org_ok = false;
    old_one.raw_block.clear();
    backup::Snapshot back;
    CHECK(backup::from_json(backup::to_json(old_one), back) && !back.alarm_ok && !back.rating_org_ok && back.raw_block.empty());
}

static void test_files()
{
    char dir[] = "/tmp/playguard_backup_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    CHECK(backup::list().empty());
    std::string err;
    const std::string first = backup::save(full(), &err);
    CHECK(!first.empty() && err.empty());
    backup::Snapshot second_snap = full();
    second_snap.level = 3;
    const std::string second = backup::save(second_snap, &err);   // same second: a suffixed name
    CHECK(!second.empty() && second != first);

    const auto names = backup::list();
    CHECK(names.size() == 2);
    CHECK(paths::backups_dir() + "/" + names[0] == second);       // newest first
    backup::Snapshot s;
    CHECK(backup::load(names[0], s) && s.level == 3);
    CHECK(backup::load(names[1], s) && s.level == 1);
    CHECK(!backup::load("missing.json", s));

    // A damaged file is listed but cannot be loaded.
    CHECK(paths::atomic_write(paths::backups_dir() + "/00000000_000000.json", "{\"format\":1"));
    CHECK(backup::list().size() == 3 && backup::list().back() == "00000000_000000.json");
    CHECK(!backup::load("00000000_000000.json", s));

    // Keeping the 2 newest deletes the oldest (the damaged one); 0 keeps all.
    CHECK(backup::prune(0) == 0 && backup::list().size() == 3);
    CHECK(backup::prune(2) == 1);
    const auto kept = backup::list();
    CHECK(kept.size() == 2 && kept == names);
    CHECK(backup::prune(5) == 0 && backup::prune(1) == 1 && backup::list().front() == names[0]);

    // A backup made with the clock in the past sorts oldest: the one just
    // written is still kept, and counts as one of the kept.
    CHECK(paths::atomic_write(paths::backups_dir() + "/29990101_000000.json", backup::to_json(full())));
    const std::string past = paths::backups_dir() + "/20000101_000000.json";
    CHECK(paths::atomic_write(past, backup::to_json(full())));
    CHECK(backup::list().size() == 3 && paths::backups_dir() + "/" + backup::list().back() == past);
    CHECK(backup::prune(2, past) == 1);
    const auto after = backup::list();
    CHECK(after.size() == 2 && after[0] == "29990101_000000.json" && after[1] == "20000101_000000.json");
    CHECK(backup::prune(1, past) == 1 && backup::list() == std::vector<std::string>{ "20000101_000000.json" });

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
}

int main()
{
    test_round_trip();
    test_validation();
    test_files();
    return CHECK_DONE("backup format, validation and file assertions passed");
}
