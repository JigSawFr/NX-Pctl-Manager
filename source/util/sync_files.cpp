// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/sync_files.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "util/paths.hpp"

namespace sync_files
{

std::string conf_file()
{
    return paths::data_dir() + "/sync.conf";
}

std::string dir()
{
    return paths::data_dir() + "/sync";
}

bool exists()
{
    struct stat st;
    return stat(conf_file().c_str(), &st) == 0;
}

SyncConf load()
{
    SyncConf c;
    sync_conf_defaults(&c);
    std::string text;
    if (paths::read_file(conf_file(), text)) sync_conf_parse(&c, text.data(), text.size());
    return c;
}

bool save(const SyncConf& c, std::string* error)
{
    std::string text(8192, '\0');
    const size_t n = sync_conf_write(&c, &text[0], text.size());
    if (n == 0) {
        if (error) *error = "the settings do not fit";
        return false;
    }
    text.resize(n);
    paths::ensure_dir(paths::data_dir());
    return paths::atomic_write(conf_file(), text, error);
}

bool ensure_id(SyncConf& c, const uint8_t random[4])
{
    if (sync_conf_id_valid(c.console_id)) return false;
    std::snprintf(c.console_id, sizeof(c.console_id), "%02x%02x%02x%02x", random[0], random[1], random[2], random[3]);
    return true;
}

SyncRecords records_from(const config::Config& c)
{
    SyncRecords r;
    sync_records_clear(&r);
    if (c.extra_weekday >= 0 && c.extra_weekday <= 6 && c.extra_date.size() == 10) {
        r.extra_weekday = (int8_t)c.extra_weekday;
        std::snprintf(r.extra_date, sizeof(r.extra_date), "%s", c.extra_date.c_str());
        r.extra_base = (uint16_t)c.extra_base;
        r.extra_value = (uint16_t)c.extra_value;
    }
    r.console_lock = c.console_lock;
    r.console_lock_prev_ok = c.console_lock_prev.size() == 7;
    for (size_t i = 0; r.console_lock_prev_ok && i < 7; i++) r.console_lock_prev[i] = (uint16_t)c.console_lock_prev[i];
    r.relock_pending = c.relock_pending;
    return r;
}

bool records_into(const SyncRecords& r, config::Config& c)
{
    const config::Config before = c;
    if (r.extra_weekday >= 0 && r.extra_weekday <= 6) {
        c.extra_weekday = r.extra_weekday;
        c.extra_date = r.extra_date;
        c.extra_base = r.extra_base;
        c.extra_value = r.extra_value;
    } else {
        c.extra_weekday = -1;
        c.extra_date.clear();
    }
    c.console_lock = r.console_lock;
    c.console_lock_prev.clear();
    if (r.console_lock_prev_ok) c.console_lock_prev.assign(r.console_lock_prev, r.console_lock_prev + 7);
    c.relock_pending = r.relock_pending;
    return before.extra_weekday != c.extra_weekday || before.extra_date != c.extra_date ||
           before.extra_base != c.extra_base || before.extra_value != c.extra_value ||
           before.console_lock != c.console_lock || before.console_lock_prev != c.console_lock_prev ||
           before.relock_pending != c.relock_pending;
}

// A value on one line: line breaks would start another key.
static std::string one_line(const std::string& s)
{
    std::string out;
    for (char ch : s) out += (ch == '\n' || ch == '\r') ? ' ' : ch;
    return out;
}

namespace
{
std::string s_firmware;
bool s_read_only = true;   // until set_console() says otherwise
}   // namespace

void set_console(const std::string& firmware, bool read_only)
{
    s_firmware = firmware;
    s_read_only = read_only;
}

std::string nro_state_text(const config::Config& c)
{
    const SyncRecords r = records_from(c);
    char buf[512];
    const size_t n = sync_records_write(&r, buf, sizeof(buf));
    std::string out = "# Written by PlayGuard for the agent sysmodule (docs/sync-protocol.md).\n";
    out.append(buf, n);
    out += "extra_auto_restore=" + std::string(c.extra_auto_restore ? "1" : "0") + "\n";
    out += "fw_gate_fw=" + one_line(c.fw_gate_fw) + "\n";
    out += "fw_gate_app=" + one_line(c.fw_gate_app) + "\n";
    out += "fw_gate_choice=" + one_line(c.fw_gate_choice) + "\n";
    out += "pin_lock=" + one_line(c.pin_lock) + "\n";
    out += "read_only=" + std::string(s_read_only || s_firmware.empty() ? "1" : "0") + "\n";
    out += "firmware=" + one_line(s_firmware) + "\n";
    return out;
}

void export_nro_state()
{
    if (!exists()) return;
    paths::ensure_dir(dir());
    std::string err;
    paths::atomic_write(dir() + "/nro_state.txt", nro_state_text(config::get()), &err);
}

std::string profiles_text(const std::vector<profiles::Profile>& list)
{
    std::string out = "# Written by PlayGuard: the saved profiles, minutes Sunday first (65535: no limit).\n";
    for (const auto& p : list) {
        const std::string name = one_line(p.name);
        if (name.empty()) continue;
        for (size_t i = 0; i < 7; i++) out += (i ? "," : "") + std::to_string(p.days[i]);
        out += "=" + name + "\n";
    }
    return out;
}

bool write_profiles(const std::vector<profiles::Profile>& list)
{
    paths::ensure_dir(dir());
    return paths::atomic_write(dir() + "/profiles.txt", profiles_text(list));
}

std::string names_text(const std::vector<std::pair<uint64_t, std::string>>& names)
{
    std::string out = "# Written by PlayGuard: game names for the agent's \"now playing\".\n";
    for (const auto& n : names) {
        const std::string name = one_line(n.second);
        if (name.empty()) continue;
        char id[17];
        std::snprintf(id, sizeof(id), "%016llX", (unsigned long long)n.first);
        out += std::string(id) + "=" + name + "\n";
    }
    return out;
}

bool write_names(const std::vector<std::pair<uint64_t, std::string>>& names)
{
    paths::ensure_dir(dir());
    return paths::atomic_write(dir() + "/names.txt", names_text(names));
}

namespace
{
std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t at = 0;
    for (;;) {
        const size_t end = s.find(sep, at);
        out.push_back(s.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (end == std::string::npos) return out;
        at = end + 1;
    }
}

bool parse_int(const std::string& s, long long lo, long long hi, long long* out)
{
    if (s.empty() || s.size() > 20) return false;
    char* end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (*end || v < lo || v > hi) return false;
    *out = v;
    return true;
}

bool parse_values(const std::string& s, size_t n, std::vector<int>* out)
{
    out->clear();
    if (!n) return s.empty();
    for (const std::string& part : split(s, ',')) {
        long long v = 0;
        if (!parse_int(part, -1, 65535, &v)) return false;
        out->push_back((int)v);
    }
    return out->size() == n;
}
}   // namespace

bool agent_records_newer()
{
    struct stat agent, nro;
    if (stat((dir() + "/agent_state.txt").c_str(), &agent) != 0) return false;
    if (stat((dir() + "/nro_state.txt").c_str(), &nro) != 0) return true;
    return agent.st_mtime >= nro.st_mtime;
}

std::vector<AgentEvent> parse_agent_events(const std::string& text)
{
    std::vector<AgentEvent> out;
    for (std::string line : split(text, '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> f = split(line, '\t');
        // The console lock's field came last; a line without it is older.
        if (f.size() != 10 && f.size() != 11) continue;
        AgentEvent e;
        long long ts = 0, applied = 0, change = 0, n = 0, lock = -1;
        if (!parse_int(f[0], 1, 0x7FFFFFFFFFFFLL, &ts) || !parse_int(f[3], 0, 1, &applied) ||
            !parse_int(f[5], 0, 64, &change) || !parse_int(f[6], 0, 7, &n))
            continue;
        if (f[1].empty() || !parse_values(f[8], (size_t)n, &e.before) || !parse_values(f[9], (size_t)n, &e.after))
            continue;
        if (f.size() == 11 && !parse_int(f[10], -1, 1, &lock)) continue;
        e.ts = (uint64_t)ts;
        e.entity = f[1];
        e.payload = f[2];
        e.applied = applied != 0;
        e.reason = f[4];
        e.change = (int)change;
        e.source = f[7].empty() ? "remote" : f[7];
        e.console_lock_after = (int)lock;
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<AgentEvent> take_agent_events()
{
    const std::string file = dir() + "/agent_events.log", taken = file + ".read";
    struct stat st;
    // A file left by a run that stopped before removing it comes first.
    if (stat(taken.c_str(), &st) != 0 && std::rename(file.c_str(), taken.c_str()) != 0) return {};
    std::string text;
    paths::read_file(taken, text);
    std::vector<AgentEvent> out = parse_agent_events(text);
    std::remove(taken.c_str());
    return out;
}

}   // namespace sync_files
