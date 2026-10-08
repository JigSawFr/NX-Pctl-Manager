// Host tests for source/util/profiles.cpp: display names with accents, the
// file name they give, collisions FAT would make, renames and damaged files.
#include <cassert>
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
    assert(sanitize_name("  School   week  ") == "School week");
    assert(sanitize_name("École\tdu lundi\n") == "Écoledu lundi");        // control characters dropped
    assert(sanitize_name("a/b\\c") == "abc");
    assert(sanitize_name("Vacances d'été ☀") == "Vacances d'été ☀");     // any printable text
    assert(sanitize_name("\xFF\xFEok") == "ok");                          // invalid UTF-8 dropped
    assert(sanitize_name(std::string(40, 'x')).size() == 32);
    assert(sanitize_name("ééééééééééééééééééééééééééééééééééé") == std::string("éééééééééééééééééééééééééééééééé"));
    assert(sanitize_name("   ").empty());

    assert(file_stem("École du lundi") == "Ecole du lundi");
    assert(file_stem("Vacances d'été ☀") == "Vacances dete");
    assert(file_stem("Cœur Ça") == "Coeur Ca");
    assert(file_stem("Straße") == "Strasse");
    assert(file_stem("周末") == "");                                      // nothing usable
    assert(file_stem("2 h - semaine_A") == "2 h - semaine_A");
}

static void test_files()
{
    using profiles::Profile;
    assert(profiles::list().empty() && profiles::count() == 0);

    std::string err;
    assert(profiles::save(make("École", 120), &err));
    assert(exists("Ecole"));
    auto all = profiles::list();
    assert(all.size() == 1 && all[0].name == "École" && all[0].file == "Ecole");
    assert(profiles::count() == 1);
    uint16_t days[7];
    for (auto& d : days) d = 120;
    assert(profiles::match(days) == "École");

    // "ecole" or "ÉCOLE" would be the same file on FAT: found before saving.
    Profile clash;
    assert(profiles::find_same_file("ecole", "", &clash) && clash.name == "École");
    assert(profiles::find_same_file("ECOLE", "", nullptr));
    assert(!profiles::find_same_file("École", "Ecole", nullptr));          // itself, when editing it
    assert(!profiles::find_same_file("Holidays", "", nullptr));

    // Edit the limits, keep the name: the same file.
    all[0].days[0] = 60;
    assert(profiles::save(all[0], &err));
    all = profiles::list();
    assert(all.size() == 1 && all[0].days[0] == 60);

    // Rename: the new file is written, the old one goes.
    all[0].name = "Semaine d'école";
    assert(profiles::save(all[0], &err));
    assert(!exists("Ecole") && exists("Semaine decole"));
    all = profiles::list();
    assert(all.size() == 1 && all[0].name == "Semaine d'école" && all[0].file == "Semaine decole");

    // Renamed by case only: still one file (FAT would make the "new" one the
    // old one, and deleting the old one would delete it).
    all[0].name = "semaine d'école";
    assert(profiles::save(all[0], &err));
    all = profiles::list();
    assert(all.size() == 1 && all[0].file == "Semaine decole" && all[0].name == "semaine d'école");

    // Nothing usable for a file name.
    err.clear();
    assert(!profiles::save(make("周末", 60), &err) && !err.empty());

    // Damaged or doubtful files are skipped; a missing name is the file name.
    const std::string dir = paths::profiles_dir();
    assert(paths::atomic_write(dir + "/fraction.json", R"({"name": "Fraction", "days": [90.5, 1, 1, 1, 1, 1, 1]})"));
    assert(paths::atomic_write(dir + "/too_long.json", R"({"name": "Too long", "days": [2000, 1, 1, 1, 1, 1, 1]})"));
    assert(paths::atomic_write(dir + "/six.json", R"({"name": "Six", "days": [1, 1, 1, 1, 1, 1]})"));
    assert(paths::atomic_write(dir + "/broken.json", R"({"name": )"));
    assert(paths::atomic_write(dir + "/Weekend.json", R"({"name": 7, "days": [null, 60, 60, 60, 60, 60, null]})"));
    all = profiles::list();
    assert(all.size() == 2);
    bool weekend = false;
    for (const auto& p : all) weekend |= p.name == "Weekend" && p.file == "Weekend" && p.days[0] == 0xFFFF && p.days[1] == 60;
    assert(weekend);

    // Delete by file, never by display name.
    assert(!profiles::remove("semaine d'école"));
    assert(!profiles::remove("../config"));
    assert(profiles::remove("Semaine decole"));
    assert(!exists("Semaine decole"));
}

int main()
{
    char dir[] = "/tmp/playguard_profiles_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_names();
    test_files();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("profiles names, file names, collisions, renames and damaged-file assertions passed");
    return 0;
}
