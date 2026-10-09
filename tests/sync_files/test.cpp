// Host tests for source/util/sync_files.cpp: sync.conf saved and read back
// (unknown lines kept), the console id made once, config.json's records to
// the shared form and back (a change reported, an incomplete record left
// out), the agent's nro_state.txt (only once the link was set up, read back
// by the agent's own parser, no line break smuggled into a value), and the
// profiles and names files.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "util/config.hpp"
#include "util/paths.hpp"
#include "util/sync_files.hpp"

static void test_conf()
{
    assert(!sync_files::exists());
    SyncConf c = sync_files::load();   // no file: the defaults
    assert(!c.enabled && c.port == 1883 && c.policy == SyncPolicy_Ask && !c.remote_timer_writes && c.ha_discovery);

    const uint8_t random[4] = { 0xA1, 0x02, 0xC3, 0x0D };
    assert(sync_files::ensure_id(c, random));
    assert(!std::strcmp(c.console_id, "a102c30d"));
    const uint8_t other[4] = { 1, 2, 3, 4 };
    assert(!sync_files::ensure_id(c, other) && !std::strcmp(c.console_id, "a102c30d"));   // made once

    c.enabled = true;
    std::snprintf(c.host, sizeof(c.host), "%s", "mqtt.lan");
    std::snprintf(c.username, sizeof(c.username), "%s", "playguard");
    std::snprintf(c.password, sizeof(c.password), "%s", "p=ss word#1");
    c.policy = SyncPolicy_Auto;
    c.remote_timer_writes = true;
    std::string err;
    assert(sync_files::save(c, &err));
    assert(sync_files::exists());

    // A key a newer version wrote, added by hand: kept by the next save.
    std::string text;
    assert(paths::read_file(sync_files::conf_file(), text));
    assert(text.find("p=ss word#1") != std::string::npos);
    assert(paths::atomic_write(sync_files::conf_file(), text + "future_key=42\n"));

    SyncConf back = sync_files::load();
    assert(back.enabled && !std::strcmp(back.host, "mqtt.lan") && !std::strcmp(back.username, "playguard"));
    assert(!std::strcmp(back.password, "p=ss word#1") && back.policy == SyncPolicy_Auto && back.remote_timer_writes);
    assert(!std::strcmp(back.console_id, "a102c30d") && !sync_conf_problem(&back));
    assert(sync_files::save(back));
    assert(paths::read_file(sync_files::conf_file(), text));
    assert(text.find("future_key=42") != std::string::npos);
}

static void test_records()
{
    config::Config c;
    c.extra_weekday = 3;
    c.extra_date = "2026-10-07";
    c.extra_base = 60;
    c.extra_value = 90;
    c.console_lock = true;
    c.console_lock_prev = { 60, 60, 60, 60, 60, 120, 65535 };
    c.relock_pending = true;

    SyncRecords r = sync_files::records_from(c);
    assert(r.extra_weekday == 3 && !std::strcmp(r.extra_date, "2026-10-07") && r.extra_base == 60 && r.extra_value == 90);
    assert(r.console_lock && r.console_lock_prev_ok && r.console_lock_prev[5] == 120 && r.console_lock_prev[6] == 65535);
    assert(r.relock_pending);

    config::Config same = c;
    assert(!sync_files::records_into(r, same));   // nothing changed

    r.extra_weekday = -1;
    r.console_lock = false;
    r.console_lock_prev_ok = false;
    r.relock_pending = false;
    assert(sync_files::records_into(r, c));
    assert(c.extra_weekday == -1 && c.extra_date.empty() && !c.console_lock && c.console_lock_prev.empty() &&
           !c.relock_pending);

    // An incomplete record is not one.
    config::Config broken;
    broken.extra_weekday = 2;
    broken.extra_date = "2026-10";
    broken.console_lock_prev = { 1, 2, 3 };
    r = sync_files::records_from(broken);
    assert(r.extra_weekday == -1 && !r.console_lock_prev_ok);
}

static void kv(void* ctx, const char* key, const char* value)
{
    auto* out = (std::string*)ctx;
    *out += std::string(key) + "=" + value + ";";
}

static void test_nro_state()
{
    config::Config c;
    c.extra_weekday = 1;
    c.extra_date = "2026-10-05";
    c.extra_base = 30;
    c.extra_value = 0;
    c.extra_auto_restore = true;
    c.fw_gate_fw = "24.0.0";
    c.fw_gate_app = "1.0.0";
    c.fw_gate_choice = "read_only\nrelock_pending=1";   // a line break would add a key
    c.pin_lock = "changes";
    const std::string text = sync_files::nro_state_text(c);

    SyncRecords r;
    sync_records_clear(&r);
    sync_records_parse(&r, text.data(), text.size());
    assert(r.extra_weekday == 1 && !std::strcmp(r.extra_date, "2026-10-05") && r.extra_base == 30 && r.extra_value == 0);
    assert(!r.relock_pending);   // the smuggled line did not become a key
    std::string keys;
    sync_kv_each(text.data(), text.size(), kv, &keys);
    assert(keys.find("extra_auto_restore=1;") != std::string::npos);
    assert(keys.find("fw_gate_fw=24.0.0;") != std::string::npos);
    assert(keys.find("fw_gate_choice=read_only relock_pending=1;") != std::string::npos);
    assert(keys.find("pin_lock=changes;") != std::string::npos);

    // Written only while sync.conf exists.
    const std::string file = sync_files::dir() + "/nro_state.txt";
    std::remove(file.c_str());
    std::remove(sync_files::conf_file().c_str());
    sync_files::export_nro_state();
    std::string read;
    assert(!paths::read_file(file, read));
    SyncConf conf = sync_files::load();
    assert(sync_files::save(conf));
    config::get() = c;
    sync_files::export_nro_state();
    assert(paths::read_file(file, read) && read == text);
}

static void test_lists()
{
    profiles::Profile school;
    school.name = "Semaine d'école";
    school.days = { 120, 60, 60, 60, 60, 60, 65535 };
    profiles::Profile empty;   // no name: left out
    const std::string p = sync_files::profiles_text({ school, empty });
    assert(p.find("120,60,60,60,60,60,65535=Semaine d'école\n") != std::string::npos);
    assert(p.find("\n=") == std::string::npos);

    const std::string n = sync_files::names_text({ { 0x0100000000010000ULL, "Super Mario Odyssey" },
                                                   { 0x01000A10041EA000ULL, "Line\nbreak" },
                                                   { 0x0100000000000001ULL, "" } });
    assert(n.find("0100000000010000=Super Mario Odyssey\n") != std::string::npos);
    assert(n.find("01000A10041EA000=Line break\n") != std::string::npos);
    assert(n.find("0100000000000001") == std::string::npos);

    assert(sync_files::write_profiles({ school }));
    assert(sync_files::write_names({ { 0x0100000000010000ULL, "Super Mario Odyssey" } }));
    std::string read;
    assert(paths::read_file(sync_files::dir() + "/profiles.txt", read) && read.find("=Semaine d'école") != std::string::npos);
    assert(paths::read_file(sync_files::dir() + "/names.txt", read) && read.find("Super Mario Odyssey") != std::string::npos);
}

int main()
{
    char dir[] = "/tmp/playguard_sync_files_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host
    assert(paths::ensure_dir(paths::data_dir()));

    test_conf();
    test_records();
    test_nro_state();
    test_lists();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("sync_files: sync.conf, records, nro_state.txt, profiles and names assertions passed");
    return 0;
}
