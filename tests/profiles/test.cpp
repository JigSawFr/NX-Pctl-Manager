// Host tests for source/util/profiles.cpp: display names with accents or in
// other scripts, the file name they give, collisions FAT would make, renames
// and damaged files.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/profiles.hpp"

static profiles::Profile make(const std::string& name, uint16_t minutes)
{
    profiles::Profile p;
    p.name = name;
    for (auto& d : p.days) d = minutes;
    return p;
}

static bool exists(const std::string& stem)
{
    std::string text;
    return paths::read_file(paths::profiles_dir() + "/" + stem + ".json", text);
}

static void test_names()
{
    using profiles::file_stem;
    using profiles::sanitize_name;
    CHECK(sanitize_name("  School   week  ") == "School week");
    CHECK(sanitize_name("École\tdu lundi\n") == "Écoledu lundi");        // control characters dropped
    CHECK(sanitize_name("a/b\\c") == "abc");
    CHECK(sanitize_name("Vacances d'été ☀") == "Vacances d'été ☀");     // any printable text
    CHECK(sanitize_name("\xFF\xFEok") == "ok");                          // invalid UTF-8 dropped
    CHECK(sanitize_name(std::string(40, 'x')).size() == 32);
    CHECK(sanitize_name("ééééééééééééééééééééééééééééééééééé") == std::string("éééééééééééééééééééééééééééééééé"));
    CHECK(sanitize_name("   ").empty());

    CHECK(file_stem("École du lundi") == "Ecole du lundi");
    CHECK(file_stem("Vacances d'été ☀") == "Vacances dete");
    CHECK(file_stem("Cœur Ça") == "Coeur Ca");
    CHECK(file_stem("Straße") == "Strasse");
    // No Latin letter or digit: "profile-" and a hash of the name, the same
    // every time, different for another name.
    const std::string cjk = file_stem("周末");
    CHECK(cjk.size() == 16 && cjk.compare(0, 8, "profile-") == 0);
    CHECK(cjk.find_first_not_of("0123456789abcdef", 8) == std::string::npos);
    CHECK(file_stem("周末") == cjk && file_stem(" 周末 ") == cjk);
    CHECK(file_stem("平日") != cjk && file_stem("Выходные") != cjk);
    CHECK(file_stem("☀ - ☀").compare(0, 8, "profile-") == 0);           // no letter or digit
    CHECK(file_stem("周末 2") == "2");
    CHECK(file_stem("   ").empty());
    CHECK(file_stem("2 h - semaine_A") == "2 h - semaine_A");
}

static void test_files()
{
    using profiles::Profile;
    CHECK(profiles::list().empty() && profiles::count() == 0);

    std::string err;
    CHECK(profiles::save(make("École", 120), &err));
    CHECK(exists("Ecole"));
    auto all = profiles::list();
    CHECK(all.size() == 1 && all[0].name == "École" && all[0].file == "Ecole");
    CHECK(profiles::count() == 1);
    uint16_t days[7];
    for (auto& d : days) d = 120;
    CHECK(profiles::match(days) == "École");

    // "ecole" or "ÉCOLE" would be the same file on FAT: found before saving.
    Profile clash;
    CHECK(profiles::find_same_file("ecole", "", &clash) && clash.name == "École");
    CHECK(profiles::find_same_file("ECOLE", "", nullptr));
    CHECK(!profiles::find_same_file("École", "Ecole", nullptr));          // itself, when editing it
    CHECK(!profiles::find_same_file("Holidays", "", nullptr));

    // Edit the limits, keep the name: the same file.
    all[0].days[0] = 60;
    CHECK(profiles::save(all[0], &err));
    all = profiles::list();
    CHECK(all.size() == 1 && all[0].days[0] == 60);

    // Rename: the new file is written, the old one goes.
    all[0].name = "Semaine d'école";
    CHECK(profiles::save(all[0], &err));
    CHECK(!exists("Ecole") && exists("Semaine decole"));
    all = profiles::list();
    CHECK(all.size() == 1 && all[0].name == "Semaine d'école" && all[0].file == "Semaine decole");

    // Renamed by case only: still one file (FAT would make the "new" one the
    // old one, and deleting the old one would delete it).
    all[0].name = "semaine d'école";
    CHECK(profiles::save(all[0], &err));
    all = profiles::list();
    CHECK(all.size() == 1 && all[0].file == "Semaine decole" && all[0].name == "semaine d'école");

    // A name without Latin letters keeps its own text and gets a hashed file.
    CHECK(profiles::save(make("周末", 60), &err));
    const std::string cjk = profiles::file_stem("周末");
    CHECK(exists(cjk));
    all = profiles::list();
    CHECK(all.size() == 2);
    bool found = false;
    for (const auto& p : all) found |= p.name == "周末" && p.file == cjk;
    CHECK(found);
    CHECK(profiles::find_same_file("周末", "", nullptr));
    CHECK(!profiles::find_same_file("平日", "", nullptr));
    CHECK(profiles::remove(cjk) && !exists(cjk));

    // An empty name is refused.
    err.clear();
    CHECK(!profiles::save(make("   ", 60), &err) && !err.empty());

    // Damaged or doubtful files are skipped; a missing name is the file name.
    const std::string dir = paths::profiles_dir();
    CHECK(paths::atomic_write(dir + "/fraction.json", R"({"name": "Fraction", "days": [90.5, 1, 1, 1, 1, 1, 1]})"));
    CHECK(paths::atomic_write(dir + "/too_long.json", R"({"name": "Too long", "days": [2000, 1, 1, 1, 1, 1, 1]})"));
    CHECK(paths::atomic_write(dir + "/six.json", R"({"name": "Six", "days": [1, 1, 1, 1, 1, 1]})"));
    CHECK(paths::atomic_write(dir + "/broken.json", R"({"name": )"));
    CHECK(paths::atomic_write(dir + "/Weekend.json", R"({"name": 7, "days": [null, 60, 60, 60, 60, 60, null]})"));
    all = profiles::list();
    CHECK(all.size() == 2);
    bool weekend = false;
    for (const auto& p : all) weekend |= p.name == "Weekend" && p.file == "Weekend" && p.days[0] == 0xFFFF && p.days[1] == 60;
    CHECK(weekend);

    // Delete by file, never by display name.
    CHECK(!profiles::remove("semaine d'école"));
    CHECK(!profiles::remove("../config"));
    CHECK(profiles::remove("Semaine decole"));
    CHECK(!exists("Semaine decole"));
}

int main()
{
    char dir[] = "/tmp/playguard_profiles_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_names();
    test_files();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
    return CHECK_DONE("profiles names, file names, collisions, renames and damaged-file assertions passed");
}
