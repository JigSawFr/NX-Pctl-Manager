// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/play_data.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <map>
#include <set>
#include <vector>

#include "util/own_time.hpp"
#include "util/paths.hpp"
#include "util/play_cache.hpp"

namespace play_data
{

namespace
{
struct Entry
{
    std::shared_ptr<const PlayStats> stats;
    std::chrono::steady_clock::time_point read_at;   // never fresh when from the SD card
    bool busy = false;
};
std::map<Key, Entry> s_entries;
std::map<int, std::function<void()>> s_listeners;
int s_next_id = 1;

std::string cache_file(const Key& key)
{
    return paths::data_dir() + "/cache/activity_" + (key.empty() ? std::string("all") : key) + ".bin";
}

// Local days between two user-clock times, or 0 when the time zone cannot say.
int days_between(u64 from, u64 to)
{
    const TimeRule* rule = time_console_rule();
    LocalTime a, b;
    if (!rule || !rule->to_local(rule->ctx, from, &a) || !rule->to_local(rule->ctx, to, &b)) return 0;
    return (int)(calendar_days_from_civil(b.year, b.month, b.day) - calendar_days_from_civil(a.year, a.month, a.day));
}

// `key`'s entry; the first time, the last run's read from the SD card, its
// day windows moved to today. Shown at once, read again as soon as asked.
Entry& entry(const Key& key)
{
    auto it = s_entries.find(key);
    if (it != s_entries.end()) return it->second;
    Entry& e = s_entries[key];
    std::string bytes;
    if (!paths::read_file(cache_file(key), bytes)) return e;
    auto data = std::make_shared<PlayStats>();
    if (!play_cache::decode(bytes, *data)) return e;
    u64 now = 0;
    time_local_now(&now, nullptr);
    if (now) play_cache::shift_days(*data, days_between(data->now, now));
    e.stats   = data;
    e.read_at = std::chrono::steady_clock::time_point::min();
    return e;
}

void save(const Key& key, const PlayStats& stats)
{
    if (R_FAILED(stats.rc)) return;   // nothing worth keeping
    std::string error;
    if (!paths::ensure_dir(paths::data_dir() + "/cache") ||
        !paths::atomic_write(cache_file(key), play_cache::encode(stats), &error))
        brls::Logger::warning("play_data: cache not saved: {}", error);
}
}   // namespace

Key key_of(const PlayAccount* account)
{
    if (!account) return "";
    return fmt::format("{:016X}{:016X}", (unsigned long long)account->uid[0], (unsigned long long)account->uid[1]);
}

std::shared_ptr<const PlayStats> latest(const Key& key)
{
    return entry(key).stats;
}

bool fresh(const Key& key, std::chrono::seconds max_age)
{
    const Entry& e = entry(key);
    return e.stats && e.read_at != std::chrono::steady_clock::time_point::min() &&
           std::chrono::steady_clock::now() - e.read_at < max_age;
}

bool busy(const Key& key)
{
    auto it = s_entries.find(key);
    return it != s_entries.end() && it->second.busy;
}

void fetch(const PlayAccount* account)
{
    const Key key = key_of(account);
    Entry& e = entry(key);
    if (e.busy) return;
    e.busy = true;
    const bool one = account != nullptr;
    const PlayAccount who = one ? *account : PlayAccount{};
    // PlayGuard's own time over a game is not play: left out (util/own_time.hpp).
    auto skip = std::make_shared<std::vector<PlayLogSpan>>(own_time::spans());
    brls::async([key, one, who, skip]() {
        auto data = std::make_shared<PlayStats>();
        playstats_fetch_skip(data.get(), one ? &who : nullptr, skip->data(), skip->size());
        brls::sync([key, data]() {
            Entry& done = s_entries[key];
            done.busy    = false;
            done.stats   = data;
            done.read_at = std::chrono::steady_clock::now();
            save(key, *data);
            // A copy: a listener may unlisten (its tab closed) while called.
            const auto listeners = s_listeners;
            for (const auto& l : listeners) l.second();
        });
    });
}

int listen(std::function<void()> on_read)
{
    const int id = s_next_id++;
    s_listeners[id] = std::move(on_read);
    return id;
}

void unlisten(int id)
{
    s_listeners.erase(id);
}

const std::vector<PlayAccount>& accounts()
{
    static bool read = false;
    static std::vector<PlayAccount> list;
    if (!read) {
        read = true;
        PlayAccount buf[PLAYSTATS_MAX_ACCOUNTS];
        Result rc = 0;
        const size_t n = playstats_accounts(buf, PLAYSTATS_MAX_ACCOUNTS, &rc);
        list.assign(buf, buf + n);
        if (R_FAILED(rc)) brls::Logger::info("play_data: accounts not listed (0x{:08X})", (unsigned)rc);
    }
    return list;
}

uint64_t today_total_s(const PlayStats& stats)
{
    uint64_t total = 0;
    for (uint32_t i = 0; i < stats.count; i++) total += stats.games[i].today_s;
    return total;
}

}   // namespace play_data
