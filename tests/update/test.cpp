// Host tests for source/util/update.cpp: version comparison, firmware strings,
// compat.json parsing and the update decision of the firmware screen.
#include <cassert>
#include <cstdio>
#include <string>

#include "util/update.hpp"

static uint32_t fw(int a, int b, int c) { return ((uint32_t)a << 16) | ((uint32_t)b << 8) | (uint32_t)c; }

static void test_versions()
{
    int v[3];
    assert(update::parse_version("1.2.3", v) && v[0] == 1 && v[1] == 2 && v[2] == 3);
    assert(update::parse_version("v10.0.42", v) && v[0] == 10 && v[2] == 42);
    assert(update::parse_version("0.0.0-dev", v) && v[0] == 0);
    assert(!update::parse_version("", v));
    assert(!update::parse_version("1.2", v));
    assert(!update::parse_version("1..3", v));
    assert(!update::parse_version("1.2.3.4", v));
    assert(!update::parse_version("1.2.x", v));
    assert(!update::parse_version("1.2.99999", v));

    assert(update::compare_versions("1.0.0", "1.0.0") == 0);
    assert(update::compare_versions("1.0.1", "1.0.0") > 0);
    assert(update::compare_versions("1.10.0", "1.9.9") > 0);    // numeric, not text
    assert(update::compare_versions("2.0.0", "10.0.0") < 0);
    assert(update::compare_versions("v1.2.0", "1.2.0") == 0);
    assert(update::compare_versions("1.0.0-dev", "1.0.0") < 0);
    assert(update::compare_versions("1.0.0", "0.0.0-dev") > 0);
    assert(update::compare_versions("garbage", "0.0.1") < 0);
    assert(update::compare_versions("0.0.1", "garbage") > 0);

    assert(update::parse_firmware("24.0.0") == fw(24, 0, 0));
    assert(update::parse_firmware("23.0.1") == fw(23, 0, 1));
    assert(update::parse_firmware("256.0.0") == 0);
    assert(update::parse_firmware("23.0") == 0);
    assert(update::parse_firmware("23.0.1-rc") == 0);
}

static void test_compat()
{
    update::Latest l;
    assert(update::parse_compat(
        R"({"schema": 1, "version": "1.1.0", "fw_tested_max": "24.0.0", "fw_min_play_timer": "21.0.0"})", &l));
    assert(l.version == "1.1.0" && l.fw_tested_max == fw(24, 0, 0));
    // Unknown fields are ignored (a later schema may add some).
    assert(update::parse_compat(R"({"version": "2.0.0", "fw_tested_max": "25.1.0", "notes": [1, 2]})", &l));
    assert(l.version == "2.0.0" && l.fw_tested_max == fw(25, 1, 0));

    update::Latest untouched;
    untouched.version = "kept";
    const char* bad[] = {
        "",
        "not json",
        "[]",
        R"({"version": "1.1.0"})",
        R"({"fw_tested_max": "24.0.0"})",
        R"({"version": 110, "fw_tested_max": "24.0.0"})",
        R"({"version": "1.1", "fw_tested_max": "24.0.0"})",
        R"({"version": "1.1.0", "fw_tested_max": "24"})",
        R"({"version": "1.1.0", "fw_tested_max": 24})",
        "<html>404</html>",
    };
    for (const char* text : bad) {
        assert(!update::parse_compat(text, &untouched));
        assert(untouched.version == "kept");
    }
}

static void test_decide()
{
    update::Latest l;
    l.version       = "1.1.0";
    l.fw_tested_max = fw(24, 0, 0);
    using update::Verdict;
    assert(update::decide(fw(24, 0, 0), "1.0.0", l) == Verdict::UpdateSupports);
    assert(update::decide(fw(23, 0, 1), "1.0.0", l) == Verdict::UpdateSupports);
    assert(update::decide(fw(24, 0, 1), "1.0.0", l) == Verdict::UpdateNoSupport);
    assert(update::decide(fw(25, 0, 0), "1.0.0", l) == Verdict::UpdateNoSupport);
    assert(update::decide(fw(25, 0, 0), "1.1.0", l) == Verdict::UpToDate);
    assert(update::decide(fw(24, 0, 0), "1.2.0", l) == Verdict::UpToDate);       // newer than the release
    assert(update::decide(fw(24, 0, 0), "0.0.0-dev", l) == Verdict::UpdateSupports);
    assert(update::decide(0, "1.0.0", l) == Verdict::UpdateNoSupport);           // unknown console firmware
}

int main()
{
    test_versions();
    test_compat();
    test_decide();
    puts("update version, compat.json and decision assertions passed");
    return 0;
}
