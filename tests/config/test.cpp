// Host tests for source/util/config.cpp and the ".tmp" recovery of
// source/util/paths.cpp: every field read on its own, out-of-range values put
// back to their defaults, records dropped as a whole, round trip, recovery
// of a save that stopped between its remove and its rename.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/config.hpp"
#include "util/paths.hpp"

static void write_config(const std::string& text)
{
    assert(paths::atomic_write(paths::config_file(), text));
}

static void test_defaults()
{
    std::remove(paths::config_file().c_str());
    config::load();
    const auto& c = config::get();
    assert(c.language == "system" && c.theme == "system" && c.auto_relock && !c.dev_mode);
    assert(c.extra_weekday == -1 && !c.relock_pending && c.fw_gate_choice.empty());

    for (const char* bad : { "", "not json", "[1, 2]", "42", "{\"language\": " }) {
        write_config(bad);
        config::get().dev_mode = true;
        config::load();
        assert(!config::get().dev_mode && config::get().language == "system");
    }
}

static void test_fields()
{
    // A wrong field keeps its default; the others are read.
    write_config(R"({"language": 5, "theme": "dark", "dev_mode": 1, "auto_relock": false,
                    "fw_gate_fw": "24.0.0", "fw_gate_app": "1.0.0", "fw_gate_choice": "risk",
                    "extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 60, "extra_value": 90,
                    "relock_pending": true, "custom_servers": ["a.example", 7, "b.example"]})");
    config::load();
    auto c = config::get();
    assert(c.language == "system" && c.theme == "dark" && !c.dev_mode && !c.auto_relock);
    assert(c.fw_gate_fw == "24.0.0" && c.fw_gate_app == "1.0.0" && c.fw_gate_choice == "risk");
    assert(c.extra_weekday == 3 && c.extra_date == "2026-10-06" && c.extra_base == 60 && c.extra_value == 90);
    assert(c.relock_pending);
    assert(c.custom_servers.size() == 2 && c.custom_servers[1] == "b.example");

    // Unknown values go back to their defaults.
    write_config(R"({"language": "de", "theme": "pink", "update_via": "ftp", "fw_gate_fw": "24.0.0",
                    "fw_gate_app": "1.0.0", "fw_gate_choice": "maybe"})");
    config::load();
    c = config::get();
    assert(c.language == "system" && c.theme == "system" && c.update_via == "auto");
    assert(c.fw_gate_choice.empty() && c.fw_gate_fw.empty() && c.fw_gate_app.empty());

    // The extra-time record is all or nothing: a base of -1 would otherwise
    // put back 0xFFFF ("no limit"), a mistyped one 0 min.
    const char* records[] = {
        R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": -1, "extra_value": 90})",
        R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 90.5, "extra_value": 90})",
        R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": "60", "extra_value": 90})",
        R"({"extra_weekday": 9, "extra_date": "2026-10-06", "extra_base": 60, "extra_value": 90})",
        R"({"extra_weekday": 3, "extra_date": "yesterday", "extra_base": 60, "extra_value": 90})",
        R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 60, "extra_value": 2000})",
        R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 60, "extra_value": 99999999999})",
    };
    for (const char* r : records) {
        write_config(r);
        config::load();
        c = config::get();
        assert(c.extra_weekday == -1 && c.extra_date.empty() && c.extra_base == 0 && c.extra_value == 0);
    }

    // Custom servers: no empty name, nothing longer than a DNS name, at most 10.
    std::string servers = "[\"\", \"" + std::string(300, 'x') + "\"";
    for (int i = 0; i < 12; i++) servers += ", \"s" + std::to_string(i) + ".example\"";
    write_config("{\"custom_servers\": " + servers + "], \"ntp_server\": \"" + std::string(300, 'y') + "\"}");
    config::load();
    c = config::get();
    assert(c.custom_servers.size() == config::MAX_CUSTOM_SERVERS && c.custom_servers[0] == "s0.example");
    assert(c.ntp_server.empty());
}

static void test_round_trip()
{
    config::Config& c = config::get();
    c = config::Config{};
    c.language = "fr";
    c.theme = "light";
    c.ntp_server = "fr.pool.ntp.org";
    c.custom_servers = { "time.example" };
    c.dev_mode = true;
    c.relock_pending = true;
    c.extra_weekday = 0;
    c.extra_date = "2026-10-05";
    c.extra_base = 120;
    c.extra_value = 150;
    assert(config::save());

    std::string text;
    assert(paths::read_file(paths::config_file(), text) && text.find("\"schema\": 1") != std::string::npos);
    c = config::Config{};
    config::load();
    assert(c.language == "fr" && c.theme == "light" && c.ntp_server == "fr.pool.ntp.org");
    assert(c.custom_servers.size() == 1 && c.dev_mode && c.relock_pending);
    assert(c.extra_weekday == 0 && c.extra_base == 120 && c.extra_value == 150);
}

static void test_tmp_recovery()
{
    // Stopped after removing config.json, before renaming config.json.tmp.
    config::get() = config::Config{};
    config::get().theme = "dark";
    assert(config::save());
    const std::string file = paths::config_file();
    assert(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    config::get() = config::Config{};
    config::load();
    assert(config::get().theme == "dark");

    // A leftover .tmp never wins over the file itself.
    write_config(R"({"theme": "light"})");
    assert(paths::atomic_write(file + ".tmp", R"({"theme": "dark"})"));
    config::load();
    assert(config::get().theme == "light");

    // Both missing: nothing to read.
    std::remove(file.c_str());
    std::remove((file + ".tmp").c_str());
    std::string text;
    assert(!paths::read_file(file, text));
}

int main()
{
    char dir[] = "/tmp/playguard_config_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_defaults();
    test_fields();
    test_round_trip();
    test_tmp_recovery();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("config fields, ranges, round trip and .tmp recovery assertions passed");
    return 0;
}
