// Host tests for source/util/backup.cpp: the JSON format, the validation that
// keeps a damaged file away from the console, and save / list / load.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

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
    assert(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

static void test_round_trip()
{
    backup::Snapshot in = full(), out;
    const std::string text = backup::to_json(in);
    assert(text.find("\"days_from_sunday\"") != std::string::npos);
    assert(text.find("null") != std::string::npos);   // no limit on Wednesday
    assert(text.find("pin") == std::string::npos);
    assert(backup::from_json(text, out) && same(in, out));

    // Values that could not be read are left out, and stay unset when read back.
    in = backup::Snapshot();
    in.vr_ok = true;
    in.vr_restricted = false;
    const std::string partial = backup::to_json(in);
    assert(partial.find("\"level\"") == std::string::npos && partial.find("play_timer") == std::string::npos);
    out = full();
    assert(backup::from_json(partial, out));
    assert(out.vr_ok && !out.vr_restricted && !out.level_ok && !out.custom_ok && !out.days_ok);

    // Nothing at all to restore: refused.
    assert(!parses(backup::to_json(backup::Snapshot())));
}

static void test_validation()
{
    assert(!parses(""));
    assert(!parses("not json"));
    assert(!parses("[]"));
    assert(!parses("{\"restrictions\":{\"level\":3}}"));                 // no format
    assert(!parses("{\"format\":2,\"restrictions\":{\"level\":3}}"));    // unknown format
    assert(parses("{\"format\":1,\"restrictions\":{\"level\":3}}"));

    assert(!parses(with("\"level\": 1", "\"level\": 5")));
    assert(!parses(with("\"level\": 1", "\"level\": -1")));
    assert(!parses(with("\"level\": 1", "\"level\": 1.5")));
    assert(!parses(with("\"level\": 1", "\"level\": \"1\"")));
    assert(!parses(with("\"rating_age\": 12", "\"rating_age\": 22")));
    assert(!parses(with("\"rating_age\": 12,", "")));                    // custom settings incomplete
    assert(!parses(with("\"sns_post_restricted\": true", "\"sns_post_restricted\": 1")));
    assert(!parses(with("\"vr_restricted\": true", "\"vr_restricted\": \"yes\"")));
    assert(!parses(with("180", "1441")));
    assert(!parses(with("180", "-5")));
    assert(!parses(with("180,", "")));                                   // six days
    assert(!parses(with("\"days_from_sunday\"", "\"days\"")));
    assert(parses(with("180", "0")));

    // The fields kept for the record are validated too.
    assert(!parses(with("\"alarm_disabled\": true", "\"alarm_disabled\": 1")));
    assert(!parses(with("\"rating_organization\": 6", "\"rating_organization\": 13")));
    assert(!parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"zz00")));
    assert(!parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"00")));       // 134 digits
    assert(parses(with("\"raw_0x44\": \"0000", "\"raw_0x44\": \"abcd")));      // either case

    // A backup from before these fields still reads.
    backup::Snapshot old_one = full();
    old_one.alarm_ok = old_one.rating_org_ok = false;
    old_one.raw_block.clear();
    backup::Snapshot back;
    assert(backup::from_json(backup::to_json(old_one), back) && !back.alarm_ok && !back.rating_org_ok && back.raw_block.empty());
}

static void test_files()
{
    char dir[] = "/tmp/playguard_backup_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    assert(backup::list().empty());
    std::string err;
    const std::string first = backup::save(full(), &err);
    assert(!first.empty() && err.empty());
    backup::Snapshot second_snap = full();
    second_snap.level = 3;
    const std::string second = backup::save(second_snap, &err);   // same second: a suffixed name
    assert(!second.empty() && second != first);

    const auto names = backup::list();
    assert(names.size() == 2);
    assert(paths::backups_dir() + "/" + names[0] == second);       // newest first
    backup::Snapshot s;
    assert(backup::load(names[0], s) && s.level == 3);
    assert(backup::load(names[1], s) && s.level == 1);
    assert(!backup::load("missing.json", s));

    // A damaged file is listed but cannot be loaded.
    assert(paths::atomic_write(paths::backups_dir() + "/00000000_000000.json", "{\"format\":1"));
    assert(backup::list().size() == 3 && backup::list().back() == "00000000_000000.json");
    assert(!backup::load("00000000_000000.json", s));

    // Keeping the 2 newest deletes the oldest (the damaged one); 0 keeps all.
    assert(backup::prune(0) == 0 && backup::list().size() == 3);
    assert(backup::prune(2) == 1);
    const auto kept = backup::list();
    assert(kept.size() == 2 && kept == names);
    assert(backup::prune(5) == 0 && backup::prune(1) == 1 && backup::list().front() == names[0]);

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
}

int main()
{
    test_round_trip();
    test_validation();
    test_files();
    std::puts("backup format, validation and file assertions passed");
    return 0;
}
