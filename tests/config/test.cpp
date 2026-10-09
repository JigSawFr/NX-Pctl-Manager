// Host tests for source/util/config.cpp and the ".tmp" recovery of
// source/util/paths.cpp: every field read on its own, out-of-range values put
// back to their defaults, records dropped as a whole, round trip, recovery
// of a save that stopped between its remove and its rename.
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
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
    assert(!c.extra_auto_restore);
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
                    "relock_pending": true, "extra_auto_restore": true,
                    "custom_servers": ["a.example", 7, "b.example"]})");
    config::load();
    auto c = config::get();
    assert(c.language == "system" && c.theme == "dark" && !c.dev_mode && !c.auto_relock);
    assert(c.fw_gate_fw == "24.0.0" && c.fw_gate_app == "1.0.0" && c.fw_gate_choice == "risk");
    assert(c.extra_weekday == 3 && c.extra_date == "2026-10-06" && c.extra_base == 60 && c.extra_value == 90);
    assert(c.relock_pending && c.extra_auto_restore);
    assert(c.custom_servers.size() == 2 && c.custom_servers[1] == "b.example");

    // Unknown values go back to their defaults.
    write_config(R"({"language": "xx", "theme": "pink", "update_via": "ftp", "fw_gate_fw": "24.0.0",
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

    // No more play today on a day without a limit: "no limit" (0xFFFF) is
    // put back the next day, so that one base outside 0..1440 is kept.
    write_config(R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 65535, "extra_value": 0})");
    config::load();
    c = config::get();
    assert(c.extra_weekday == 3 && c.extra_base == 65535 && c.extra_value == 0);
    write_config(R"({"extra_weekday": 3, "extra_date": "2026-10-06", "extra_base": 65534, "extra_value": 0})");
    config::load();
    assert(config::get().extra_weekday == -1);

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
    c.extra_auto_restore = true;
    assert(config::save());

    std::string text;
    assert(paths::read_file(paths::config_file(), text) && text.find("\"schema\": 1") != std::string::npos);
    c = config::Config{};
    config::load();
    assert(c.language == "fr" && c.theme == "light" && c.ntp_server == "fr.pool.ntp.org");
    assert(c.custom_servers.size() == 1 && c.dev_mode && c.relock_pending);
    assert(c.extra_weekday == 0 && c.extra_base == 120 && c.extra_value == 150 && c.extra_auto_restore);

    // Console lock: the flag and the seven saved limits round-trip.
    c = config::Config{};
    c.console_lock = true;
    c.console_lock_prev = { 180, 120, 120, 120, 120, 120, 0xFFFF };
    assert(config::save());
    c = config::Config{};
    config::load();
    assert(c.console_lock && c.console_lock_prev == std::vector<int>({ 180, 120, 120, 120, 120, 120, 0xFFFF }));
}

static void test_console_lock()
{
    // Saved limits must be exactly seven, each a real limit or 0xFFFF.
    const char* bad[] = {
        R"({"console_lock": true, "console_lock_prev": [0, 0, 0, 0, 0, 0]})",       // six
        R"({"console_lock": true, "console_lock_prev": [0, 0, 0, 0, 0, 0, 0, 0]})", // eight
        R"({"console_lock": true, "console_lock_prev": [0, 0, 0, 0, 0, 0, 1441]})", // out of range
        R"({"console_lock": true, "console_lock_prev": [0, 0, 0, 0, 0, 0, -1]})",
        R"({"console_lock": true, "console_lock_prev": "all"})",
    };
    for (const char* b : bad) {
        write_config(b);
        config::load();
        // The flag is kept (it is a plain bool), but the nonsense limits are dropped,
        // so turning the lock off clears the limit rather than restoring garbage.
        assert(config::get().console_lock_prev.empty());
    }
    // 0 every day (a real "locked" record) is kept.
    write_config(R"({"console_lock": true, "console_lock_prev": [0, 0, 0, 0, 0, 0, 0]})");
    config::load();
    assert(config::get().console_lock && config::get().console_lock_prev.size() == 7);
    // Default: off, nothing saved.
    config::Config d;
    assert(!d.console_lock && d.console_lock_prev.empty());
}

static void test_preferences()
{
    // Defaults.
    config::Config d;
    assert(d.start_tab == "dashboard" && d.extra_amounts == std::vector<int>({ 15, 30, 60 }));
    assert(d.activity_period == 1 && d.export_format == 0 && d.backup_keep == 0);
    assert(!d.update_daily && d.update_checked.empty() && !d.clock_check_at_start && d.pin_lock == "off");
    assert(d.onboarding_at_start);
    write_config(R"({"onboarding_at_start": false})");
    config::load();
    assert(!config::get().onboarding_at_start);
    write_config(R"({"pin_lock": "open"})");
    config::load();
    assert(config::get().pin_lock == "open");
    write_config(R"({"pin_lock": "always"})");
    config::load();
    assert(config::get().pin_lock == "off");

    // Values from the lists are kept.
    write_config(R"({"start_tab": "activity", "extra_amounts": [30, 60, 90], "activity_period": 2,
                    "export_format": 3, "backup_keep": 10, "update_daily": true,
                    "update_checked": "2026-10-07", "clock_check_at_start": true})");
    config::load();
    auto c = config::get();
    assert(c.start_tab == "activity" && c.extra_amounts == std::vector<int>({ 30, 60, 90 }));
    assert(c.activity_period == 2 && c.export_format == 3 && c.backup_keep == 10);
    assert(c.update_daily && c.update_checked == "2026-10-07" && c.clock_check_at_start);

    // Anything else goes back to the default: no 7-minute keep, no 999-minute
    // extra time, no tab that does not exist.
    write_config(R"({"start_tab": "hidden", "extra_amounts": [15, 30, 999], "activity_period": 5,
                    "export_format": -1, "backup_keep": 7, "update_checked": "yesterday"})");
    config::load();
    c = config::get();
    assert(c.start_tab == "dashboard" && c.extra_amounts == std::vector<int>({ 15, 30, 60 }));
    assert(c.activity_period == 1 && c.export_format == 0 && c.backup_keep == 0 && c.update_checked.empty());
    write_config(R"({"extra_amounts": "15,30,60", "backup_keep": "all"})");
    config::load();
    assert(config::get().extra_amounts == std::vector<int>({ 15, 30, 60 }) && config::get().backup_keep == 0);

    // 64-bit values are not narrowed into a valid one: 2^32 + 15 is not 15,
    // 2^32 is not 0 (a "locked" day), 2^64 - 1 is not -1.
    write_config(R"({"extra_amounts": [4294967311, 30, 60], "backup_keep": 4294967306,
                    "console_lock_prev": [4294967296, 0, 0, 0, 0, 0, 0],
                    "extra_weekday": 18446744073709551615, "extra_date": "2026-10-07",
                    "extra_base": 60, "extra_value": 90})");
    config::load();
    c = config::get();
    assert(c.extra_amounts == std::vector<int>({ 15, 30, 60 }) && c.backup_keep == 0);
    assert(c.console_lock_prev.empty() && c.extra_weekday == -1 && c.extra_date.empty());

    // Round trip.
    config::get() = config::Config{};
    config::get().start_tab = "tools";
    config::get().extra_amounts = { 5, 10, 15 };
    config::get().backup_keep = 20;
    assert(config::save());
    config::get() = config::Config{};
    config::load();
    assert(config::get().start_tab == "tools" && config::get().extra_amounts == std::vector<int>({ 5, 10, 15 }));
    assert(config::get().backup_keep == 20);
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

    // The next save must not truncate that .tmp, the only good copy: it is
    // promoted first, and a save that fails afterwards still leaves it.
    std::string text;
    assert(paths::atomic_write(file, R"({"theme": "light"})"));
    assert(paths::read_file(file, text) && text == R"({"theme": "light"})");
    assert(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    assert(paths::atomic_write(file, R"({"theme": "dark"})"));
    assert(paths::read_file(file, text) && text == R"({"theme": "dark"})");
    struct stat st;
    assert(stat((file + ".tmp").c_str(), &st) != 0);
    // Same, but the write fails (file size limit): the old content survives.
    assert(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    std::signal(SIGXFSZ, SIG_IGN);
    rlimit saved;
    assert(getrlimit(RLIMIT_FSIZE, &saved) == 0);
    rlimit small = saved;
    small.rlim_cur = 64;
    assert(setrlimit(RLIMIT_FSIZE, &small) == 0);
    const bool wrote = paths::atomic_write(file, std::string(4096, ' '));
    assert(setrlimit(RLIMIT_FSIZE, &saved) == 0);
    assert(!wrote);
    assert(paths::read_file(file, text) && text == R"({"theme": "dark"})");

    // A leftover .tmp never wins over the file itself.
    write_config(R"({"theme": "light"})");
    assert(paths::atomic_write(file + ".tmp", R"({"theme": "dark"})"));
    config::load();
    assert(config::get().theme == "light");

    // Both missing: nothing to read.
    std::remove(file.c_str());
    std::remove((file + ".tmp").c_str());
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
    test_console_lock();
    test_preferences();
    test_tmp_recovery();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("config fields, ranges, round trip and .tmp recovery assertions passed");
    return 0;
}
