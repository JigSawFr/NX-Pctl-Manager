// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/play_data.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <map>
#include <set>

namespace play_data
{

namespace
{
struct Entry
{
    std::shared_ptr<const PlayStats> stats;
    std::chrono::steady_clock::time_point read_at;
    bool busy = false;
};
std::map<Key, Entry> s_entries;
std::map<int, std::function<void()>> s_listeners;
int s_next_id = 1;
}   // namespace

Key key_of(const PlayAccount* account)
{
    if (!account) return "";
    return fmt::format("{:016X}{:016X}", (unsigned long long)account->uid[0], (unsigned long long)account->uid[1]);
}

std::shared_ptr<const PlayStats> latest(const Key& key)
{
    auto it = s_entries.find(key);
    return it == s_entries.end() ? nullptr : it->second.stats;
}

bool fresh(const Key& key, std::chrono::seconds max_age)
{
    auto it = s_entries.find(key);
    return it != s_entries.end() && it->second.stats &&
           std::chrono::steady_clock::now() - it->second.read_at < max_age;
}

bool busy(const Key& key)
{
    auto it = s_entries.find(key);
    return it != s_entries.end() && it->second.busy;
}

void fetch(const PlayAccount* account)
{
    const Key key = key_of(account);
    Entry& e = s_entries[key];
    if (e.busy) return;
    e.busy = true;
    const bool one = account != nullptr;
    const PlayAccount who = one ? *account : PlayAccount{};
    brls::async([key, one, who]() {
        auto data = std::make_shared<PlayStats>();
        playstats_fetch_for(data.get(), one ? &who : nullptr);
        brls::sync([key, data]() {
            Entry& done = s_entries[key];
            done.busy    = false;
            done.stats   = data;
            done.read_at = std::chrono::steady_clock::now();
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
