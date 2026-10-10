// Host tests for source/util/pt_block.cpp: the hex form the backups share,
// field names along pure.h's layout, the diff and the reference file.
#include "check.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/pt_block.hpp"

using pt_block::Block;

// The block read on a console with a 2 h limit on Sun and Sat (pure.h).
static Block observed()
{
    Block b{};
    b[0] = 0x0101;
    b[1] = 0x0001;
    for (int day : { 0, 6 }) {
        b[7 + 4 * day]     = 0x0600;
        b[7 + 4 * day + 1] = 0x0100;
        b[7 + 4 * day + 2] = 120;
    }
    return b;
}

static void test_hex()
{
    const Block b = observed();
    const std::string hex = pt_block::to_hex(b);
    CHECK(hex.size() == 136);
    CHECK(hex.compare(0, 8, "01010001") == 0);
    CHECK(hex.compare(28, 12, "060001000078") == 0);   // Sun: [+0], flag, 120 min

    Block back{};
    CHECK(pt_block::from_hex(hex, &back) && back == b);
    std::string lower = hex;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
    CHECK(pt_block::from_hex(lower, &back) && back == b);

    Block untouched{};
    untouched[0] = 7;
    CHECK(!pt_block::from_hex(hex.substr(1), &untouched) && untouched[0] == 7);   // 135 digits
    CHECK(!pt_block::from_hex(hex + "0", &untouched));
    std::string bad = hex;
    bad[10] = 'g';
    CHECK(!pt_block::from_hex(bad, &untouched) && untouched[0] == 7);
}

static void test_names_and_diff()
{
    const auto& en = pt_block::english_days();
    CHECK(pt_block::field_name(0, en) == "header [0]");
    CHECK(pt_block::field_name(1, en) == "header [1]");
    CHECK(pt_block::field_name(4, en) == "[4] (reserved)");
    CHECK(pt_block::field_name(7, en) == "Sun [+0]");
    CHECK(pt_block::field_name(8, en) == "Sun flag");
    CHECK(pt_block::field_name(13, en) == "Mon minutes");
    CHECK(pt_block::field_name(14, en) == "Mon [+3]");
    CHECK(pt_block::field_name(33, en) == "Sat minutes");   // the last group has no [+3]
    const std::array<std::string, 7> fr = { "dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam." };
    CHECK(pt_block::field_name(9, fr) == "dim. minutes");

    const Block before = observed();
    CHECK(pt_block::diff(before, before).empty());
    Block after = before;
    after[1]  = 0x0003;   // e.g. a mode the companion app sets
    after[33] = 90;       // Sat 2 h -> 1 h 30
    const auto changes = pt_block::diff(before, after);
    CHECK(changes.size() == 2);
    CHECK(changes[0].index == 1 && changes[0].before == 0x0001 && changes[0].after == 0x0003);
    CHECK(changes[1].index == 33 && changes[1].before == 120 && changes[1].after == 90);

    pt_block::Reference ref;
    ref.block    = before;
    ref.saved_at = "2026-10-08 14:03";
    ref.firmware = "22.1.0";
    const std::string text = pt_block::report(ref, after, "2026-10-08 14:20");
    CHECK(text.find("Reference: 2026-10-08 14:03 (firmware 22.1.0)") != std::string::npos);
    CHECK(text.find("u16[ 1] header [1]       0001 -> 0003  (1 -> 3)") != std::string::npos);
    CHECK(text.find("u16[33] Sat minutes      0078 -> 005A  (120 -> 90)") != std::string::npos);
    CHECK(text.find("reference " + pt_block::to_hex(before)) != std::string::npos);
    CHECK(text.find("now       " + pt_block::to_hex(after)) != std::string::npos);
    CHECK(pt_block::report(ref, before, "x").find("No value changed.") != std::string::npos);
}

static void test_reference_file()
{
    pt_block::Reference none;
    CHECK(!pt_block::load_reference(&none));   // nothing saved yet

    pt_block::Reference ref;
    ref.block    = observed();
    ref.saved_at = "2026-10-08 14:03";
    ref.firmware = "22.1.0";
    std::string err;
    CHECK(pt_block::save_reference(ref, &err));
    pt_block::Reference back;
    CHECK(pt_block::load_reference(&back));
    CHECK(back.block == ref.block && back.saved_at == ref.saved_at && back.firmware == ref.firmware);

    // Damaged or foreign files are not a reference.
    const char* damaged[] = {
        "not json",
        "[]",
        "{\"block\": \"0101\"}",
        "{\"format\": \"playguard-play-timer-block\", \"block\": \"0101\"}",
        "{\"format\": \"playguard-play-timer-block\", \"block\": 5}",
        "{\"format\": 3, \"block\": \"x\"}",
    };
    for (const char* text : damaged) {
        CHECK(paths::atomic_write(pt_block::reference_path(), text));
        pt_block::Reference r;
        r.saved_at = "kept";
        CHECK(!pt_block::load_reference(&r) && r.saved_at == "kept");
    }
}

int main()
{
    char dir[] = "/tmp/playguard_pt_block_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_hex();
    test_names_and_diff();
    test_reference_file();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
    return CHECK_DONE("play-timer block hex, field names, diff and reference file assertions passed");
}
