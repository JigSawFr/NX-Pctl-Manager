// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/sync_flow.hpp"

#include <atomic>
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fmt/format.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "action/play_data.hpp"
#include "action/sync_orders.hpp"
#include "activity/lock_activity.hpp"
#include "app.hpp"
#include "sync/sync_net.h"
#include "sync/sync_state.h"
#include "sync/sync_tls.h"
#include "tab/tab_base.hpp"
#include "ui/ui.hpp"
#include "util/agent_client.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/profiles.hpp"
#include "util/sync_files.hpp"

namespace sync_flow
{

namespace
{
using Clock = std::chrono::steady_clock;

constexpr size_t LOG_LINES = 100;
constexpr size_t REPORT_MAX = 128 * 1024;
constexpr auto FETCH_EVERY = std::chrono::minutes(5);
constexpr auto ACCOUNTS_FRESH = std::chrono::minutes(15);
constexpr auto EXIT_WAIT = std::chrono::seconds(4);
constexpr auto PROBE_EVERY = std::chrono::seconds(5);

// What the UI thread and the worker share. The worker never calls into
// borealis but brls::sync() and the logger; the UI never calls the engine.
struct Shared
{
    std::mutex m;
    std::condition_variable cv;
    bool stop = false, wake = false, finished = false;

    // UI -> worker
    bool reconf = false;
    SyncConf conf{};
    bool sync_now = false, state_changed = false, activity_changed = false, discovery_changed = false;
    std::string state, activity;            // the documents the engine publishes
    Clock::time_point state_at{};
    std::vector<std::string> profiles;
    std::map<std::string, std::pair<std::string, bool>> pubs;   // sub -> payload, retained (sent once online)
    std::deque<std::pair<uint32_t, SyncOutcome>> done;
    std::atomic<bool> read_only{ false };
    uint64_t posix_base = 0;
    Clock::time_point posix_at{};

    // worker -> UI
    std::deque<sync_orders::Order> orders;
    bool conf_saved = false;
    SyncConf saved{};
    bool state_wanted = false;
    SyncStatus status{};
    size_t pending = 0;
    std::deque<std::string> log;
};

// ---- the worker (its own thread) ----

size_t copy_out(const std::string& doc, char* out, size_t cap)
{
    if (doc.empty() || doc.size() >= cap) return 0;
    std::memcpy(out, doc.data(), doc.size());
    return doc.size();
}

size_t host_state(void* ctx, char* out, size_t cap)
{
    auto* sh = (Shared*)ctx;
    std::lock_guard<std::mutex> lk(sh->m);
    // Older than a few seconds (Home Assistant's "Sync now", a reconnection):
    // the UI reads the console again and the newer state follows.
    if (Clock::now() - sh->state_at > std::chrono::seconds(5)) sh->state_wanted = true;
    return copy_out(sh->state, out, cap);
}

size_t host_activity(void* ctx, char* out, size_t cap)
{
    auto* sh = (Shared*)ctx;
    std::lock_guard<std::mutex> lk(sh->m);
    return copy_out(sh->activity, out, cap);
}

void pump();

bool host_order(void* ctx, uint32_t id, const SyncIntent* intent, const char* entity, const char* payload,
                bool retained, SyncOutcome*)
{
    auto* sh = (Shared*)ctx;
    sync_orders::Order o;
    o.id = id;
    o.intent = *intent;
    o.entity = entity;
    o.payload = payload;
    o.retained = retained;
    {
        std::lock_guard<std::mutex> lk(sh->m);
        sh->orders.push_back(o);
    }
    brls::sync([]() { pump(); });
    return false;   // answered from the UI thread (sync_engine_order_done)
}

size_t host_profiles(void* ctx, char (*names)[SYNC_PROFILE_MAX], size_t max)
{
    auto* sh = (Shared*)ctx;
    std::lock_guard<std::mutex> lk(sh->m);
    size_t n = 0;
    for (const auto& p : sh->profiles) {
        if (n == max) break;
        if (p.size() >= SYNC_PROFILE_MAX) continue;
        std::memcpy(names[n], p.c_str(), p.size() + 1);
        n++;
    }
    return n;
}

void host_conf_changed(void* ctx, const SyncConf* conf)
{
    auto* sh = (Shared*)ctx;
    {
        std::lock_guard<std::mutex> lk(sh->m);
        sh->saved = *conf;
        sh->conf_saved = true;
    }
    brls::sync([]() { pump(); });
}

bool host_read_only(void* ctx)
{
    return ((Shared*)ctx)->read_only.load();
}

uint64_t host_now_posix(void* ctx)
{
    auto* sh = (Shared*)ctx;
    std::lock_guard<std::mutex> lk(sh->m);
    if (!sh->posix_base) return 0;
    return sh->posix_base + (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - sh->posix_at).count();
}

void host_log(void* ctx, const char* line)
{
    auto* sh = (Shared*)ctx;
    brls::Logger::info("sync: {}", line);
    std::lock_guard<std::mutex> lk(sh->m);
    sh->log.push_back(ui::now_hms() + "  " + line);
    while (sh->log.size() > LOG_LINES) sh->log.pop_front();
}

struct Link
{
    SyncEngine engine;
    SyncTcp    tcp;
    SyncTls    tls;
};

SyncIo io_for(Link& l, const SyncConf& c)
{
    l.tcp.fd = -1;
    std::memset(&l.tls, 0, sizeof(l.tls));
    l.tls.tcp.fd = -1;
    return c.tls ? sync_tls_io(&l.tls, c.ca_file) : sync_tcp_io(&l.tcp);
}

void worker(std::shared_ptr<Shared> sh, SyncConf conf, std::string version)
{
    SyncHost host{};
    host.ctx = sh.get();
    host.state = host_state;
    host.activity = host_activity;
    host.order = host_order;
    host.report = nullptr;   // the UI thread writes it (an order)
    host.profiles = host_profiles;
    host.conf_changed = host_conf_changed;
    host.read_only = host_read_only;
    host.now_posix = host_now_posix;
    host.log = host_log;

    // About 40 KiB: on the heap, not on this thread's stack.
    std::unique_ptr<Link> link(new Link());
    sync_engine_init(&link->engine, &conf, io_for(*link, conf), &host, sync_now_ms, "app", version.c_str());

    for (;;) {
        bool reconf = false, now = false, state = false, activity = false, discovery = false;
        SyncConf next{};
        std::deque<std::pair<uint32_t, SyncOutcome>> done;
        std::map<std::string, std::pair<std::string, bool>> pubs;
        {
            std::lock_guard<std::mutex> lk(sh->m);
            if (sh->stop) break;
            reconf = sh->reconf;
            next = sh->conf;
            now = sh->sync_now;
            state = sh->state_changed;
            activity = sh->activity_changed;
            discovery = sh->discovery_changed;
            sh->reconf = sh->sync_now = sh->state_changed = sh->activity_changed = sh->discovery_changed = false;
            done.swap(sh->done);
            if (sync_engine_online(&link->engine)) pubs.swap(sh->pubs);
        }
        SyncEngine* e = &link->engine;
        if (reconf) {
            if (next.tls != conf.tls || std::strcmp(next.ca_file, conf.ca_file)) {
                // Another stream under the session: a new one. Its order
                // ids go on from the old one's, so an answer still on its
                // way from the UI never lands on another order.
                const uint32_t next_id = e->next_id;
                sync_engine_stop(e);
                conf = next;
                sync_engine_init(e, &conf, io_for(*link, conf), &host, sync_now_ms, "app", version.c_str());
                e->next_id = next_id;
            } else {
                conf = next;
                sync_engine_reconfigure(e, &conf);
            }
        }
        for (const auto& d : done) sync_engine_order_done(e, d.first, &d.second);
        for (const auto& p : pubs)
            sync_engine_publish(e, p.first.c_str(), p.second.first.data(), p.second.first.size(), p.second.second);
        if (now) sync_engine_sync_now(e);
        if (state) sync_engine_state_changed(e);
        if (activity) sync_engine_activity_changed(e);
        if (discovery) sync_engine_discovery_changed(e);

        const int sleep = sync_engine_step(e, 250);

        std::unique_lock<std::mutex> lk(sh->m);
        sh->status = *sync_engine_status(e);
        sh->pending = sync_engine_pending(e);
        if (sleep > 0 && !sh->stop && !sh->wake)
            sh->cv.wait_for(lk, std::chrono::milliseconds(sleep > 1000 ? 1000 : sleep),
                            [&]() { return sh->stop || sh->wake; });
        sh->wake = false;
    }
    sync_engine_stop(&link->engine);   // "offline", DISCONNECT
    std::lock_guard<std::mutex> lk(sh->m);
    sh->status = *sync_engine_status(&link->engine);
    sh->status.state = SyncLink_Off;
    sh->finished = true;
    sh->cv.notify_all();
}

// ---- the UI thread ----

std::shared_ptr<Shared> s_shared;
std::thread s_worker;
SyncConf s_conf{};
bool s_started = false;
brls::RepeatingTimer* s_timer = nullptr;
int s_listen = 0;
bool s_dirty = true;
Clock::time_point s_last_state{}, s_last_fetch{};
std::string s_state_body;   // the last state without its time stamp
std::deque<sync_orders::Order> s_queue;
bool s_order_running = false;
std::set<std::string> s_finals_sent;
bool s_old_cleared = false;
std::string s_names_sent, s_week_sent;
std::vector<std::string> s_profiles_sent;
bool s_profiles_known = false;
int s_read_only_sent = -1;   // the discovery offers no control in read-only mode
uint32_t s_generation = 0;   // changes with the session: an answer never reaches another one

// Agent mode: the agent sysmodule holds the link (sync/agent_ipc.h, its
// pg:agent service); PlayGuard pushes what it reads of the console and carries
// out the orders the agent hands it. No worker then.
bool s_agent = false;           // PlayGuard's session on pg:agent
bool s_agent_refused = false;   // the agent runs but speaks another protocol: it keeps the link to itself
AgentHelloReply s_hello{};
AgentStatus s_agent_status{};
int s_agent_fg = -1;            // what the agent was last told (-1: nothing yet)
Clock::time_point s_agent_state_at{}, s_last_probe{};
bool s_hold = false;            // an update of the agent runs
std::string s_agent_activity;
std::deque<std::pair<std::string, std::string>> s_agent_finals;   // date, document: waiting for room

void wake(Shared& sh)
{
    sh.wake = true;
    sh.cv.notify_all();
}

// The link runs from here: PlayGuard's own session, or the agent's.
bool running()
{
    return s_shared != nullptr || s_agent;
}

bool wanted()
{
    return s_conf.enabled && !sync_conf_problem(&s_conf);
}

void agent_lost();

// A pg:agent call's result: false when it failed. A session that went away
// (the agent stopped) is dropped; the next probe hands the link to PlayGuard.
bool agent_ok(Result rc, const char* what)
{
    if (R_SUCCEEDED(rc)) return true;
    brls::Logger::warning("sync: agent {}: 0x{:08X}", what, (unsigned)rc);
    if (!agent_client::connected()) agent_lost();
    return false;
}

// Finished days wait for room in the agent (it holds a few until published).
void agent_flush_finals()
{
    while (s_agent && !s_agent_finals.empty()) {
        const Result rc = agent_client::push_final(s_agent_finals.front().first, s_agent_finals.front().second);
        if (rc == AGENT_RC_FULL) return;
        if (!agent_ok(rc, "final") && !s_agent) return;   // gone (the list went with it)
        s_agent_finals.pop_front();                       // sent, or refused (too large): dropped
    }
}

void post_publish(const std::string& sub, const std::string& payload, bool retained)
{
    if (s_agent) {
        if (sub == "names") agent_ok(agent_client::push(AgentCmd_PushNames, payload), "names");
        else if (sub == "week") agent_ok(agent_client::push(AgentCmd_PushWeek, payload), "week");
        else if (sub.compare(0, 9, "activity/") == 0) {
            s_agent_finals.push_back({ sub.substr(9), payload });
            agent_flush_finals();
        }
        // A report: the agent writes and publishes its own.
        return;
    }
    if (!s_shared) return;
    std::lock_guard<std::mutex> lk(s_shared->m);
    s_shared->pubs[sub] = { payload, retained };
    wake(*s_shared);
}

std::string date_text(int year, unsigned month, unsigned day)
{
    return fmt::format("{:04d}-{:02d}-{:02d}", year, month, day);
}

// The local date `back` days before today ("" when the clock cannot say).
std::string date_before(const LocalTime& today, int back)
{
    if (!today.year) return "";
    const s64 days = calendar_days_from_civil(today.year, today.month, today.day) - back;
    int y;
    unsigned m, d;
    calendar_civil_from_days(days, &y, &m, &d);
    return date_text(y, m, d);
}

std::string hex16(uint64_t v)
{
    return fmt::format("{:016X}", (unsigned long long)v);
}

// The state document, without its time stamp, to tell a change.
std::string without_ts(const std::string& doc)
{
    const size_t at = doc.find("\"ts\":");
    if (at == std::string::npos) return doc;
    size_t end = at + 5;
    while (end < doc.size() && doc[end] >= '0' && doc[end] <= '9') end++;
    return doc.substr(0, at) + doc.substr(end);
}

// Every account's data, when each was read by this run not long ago (the
// Activity tab's account filter reads them); else none: a partial split
// would be wrong.
std::vector<std::pair<PlayAccount, std::shared_ptr<const PlayStats>>> accounts_fresh()
{
    std::vector<std::pair<PlayAccount, std::shared_ptr<const PlayStats>>> out;
    const auto& list = play_data::accounts();
    if (list.empty()) return out;
    for (const auto& a : list) {
        const play_data::Key key = play_data::key_of(&a);
        if (!play_data::fresh(key, std::chrono::duration_cast<std::chrono::seconds>(ACCOUNTS_FRESH))) return {};
        out.push_back({ a, play_data::latest(key) });
    }
    return out;
}

// Today's activity (k = 0) or a finished day's (k = 1..6).
std::string activity_doc(const PlayStats& stats, int k, const std::string& date, uint64_t ts)
{
    std::vector<SyncAppTime> apps;
    for (uint32_t i = 0; i < stats.count; i++) {
        const uint32_t s = k == 0 ? stats.games[i].today_s : stats.games[i].day_s[k];
        if (s) apps.push_back({ stats.games[i].app_id, s });
    }
    std::vector<SyncAccountTime> accounts;
    const auto fresh = accounts_fresh();
    for (const auto& a : fresh) {
        SyncAccountTime t{};
        t.uid[0] = a.first.uid[0];
        t.uid[1] = a.first.uid[1];
        for (uint32_t i = 0; a.second && i < a.second->count; i++)
            t.seconds += k == 0 ? a.second->games[i].today_s : a.second->games[i].day_s[k];
        accounts.push_back(t);
    }
    SyncActivity act{};
    act.source = "app";
    act.ts = ts;
    act.local_date = date.c_str();
    act.final = k > 0;
    act.apps = apps.data();
    act.n_apps = apps.size();
    act.accounts = fresh.empty() ? nullptr : accounts.data();
    act.n_accounts = accounts.size();
    // While PlayGuard is in front, no game is being played.
    act.now_playing = 0;
    std::string out(16384 + apps.size() * 64, '\0');
    const size_t n = sync_activity_build(&act, &out[0], out.size());
    out.resize(n);
    return out;
}

std::string names_doc(const PlayStats& stats, uint64_t ts)
{
    std::vector<SyncAppName> apps;
    for (uint32_t i = 0; i < stats.count; i++)
        if (stats.games[i].name[0]) apps.push_back({ stats.games[i].app_id, stats.games[i].name });
    std::vector<SyncAccountName> accounts;
    for (const auto& a : play_data::accounts()) accounts.push_back({ { a.uid[0], a.uid[1] }, a.nickname });
    std::string out(8192 + apps.size() * 300, '\0');
    const size_t n = sync_names_build(ts, apps.data(), apps.size(), accounts.data(), accounts.size(), &out[0], out.size());
    out.resize(n);
    return out;
}

std::string week_doc(const PlayStats& stats, const LocalTime& today, uint64_t ts)
{
    nlohmann::json j;
    j["schema"] = SYNC_SCHEMA;
    j["ts"] = ts;
    nlohmann::json days = nlohmann::json::array();
    for (int k = 6; k >= 0; k--) days.push_back(date_before(today, k));
    j["days"] = days;
    nlohmann::json games = nlohmann::json::array();
    for (uint32_t i = 0; i < stats.count; i++) {
        const GameStat& g = stats.games[i];
        if (!g.week_s) continue;
        nlohmann::json min = nlohmann::json::array();
        for (int k = 6; k >= 0; k--) min.push_back(g.day_s[k] / 60);
        nlohmann::json game = { { "app_id", hex16(g.app_id) }, { "min", min } };
        game["total_min"] = g.totals_ok ? nlohmann::json(g.total_s / 60) : nlohmann::json(nullptr);
        game["launches"] = g.totals_ok ? nlohmann::json(g.launches) : nlohmann::json(nullptr);
        games.push_back(game);
    }
    j["games"] = games;
    nlohmann::json accounts = nlohmann::json::array();
    for (const auto& a : accounts_fresh()) {
        nlohmann::json min = nlohmann::json::array();
        for (int k = 6; k >= 0; k--) {
            uint32_t s = 0;
            for (uint32_t i = 0; a.second && i < a.second->count; i++) s += a.second->games[i].day_s[k];
            min.push_back(s / 60);
        }
        accounts.push_back({ { "uid", hex16(a.first.uid[0]) + hex16(a.first.uid[1]) }, { "min", min } });
    }
    j["accounts"] = accounts;
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

// After each read of the play log (this run's, for every account): today's
// activity, the names, the week, the finished days.
void on_play_data()
{
    if (!running() || !play_data::fresh("", std::chrono::seconds(60))) return;
    auto stats = play_data::latest("");
    if (!stats || R_FAILED(stats->rc) || !stats->windows_ok) return;
    u64 posix = 0;
    LocalTime today{};
    if (!time_local_now(&posix, &today)) return;
    const std::string date = date_text(today.year, today.month, today.day);

    const std::string activity = activity_doc(*stats, 0, date, posix);
    if (s_agent) {
        if (activity != s_agent_activity && agent_ok(agent_client::push(AgentCmd_PushActivity, activity), "activity"))
            s_agent_activity = activity;
        if (!running()) return;
    } else {
        std::lock_guard<std::mutex> lk(s_shared->m);
        if (s_shared->activity != activity) s_shared->activity_changed = true;
        s_shared->activity = activity;
        wake(*s_shared);
    }
    const std::string names = names_doc(*stats, posix);
    if (without_ts(names) != without_ts(s_names_sent)) {
        s_names_sent = names;
        post_publish("names", names, true);
        std::vector<std::pair<uint64_t, std::string>> list;
        for (uint32_t i = 0; i < stats->count; i++)
            if (stats->games[i].name[0]) list.push_back({ stats->games[i].app_id, stats->games[i].name });
        sync_files::write_names(list);   // for the agent's "now playing"
    }
    const std::string week = week_doc(*stats, today, posix);
    if (without_ts(week) != without_ts(s_week_sent)) {
        s_week_sent = week;
        post_publish("week", week, true);
    }
    for (int k = 1; k <= 6; k++) {
        const std::string day = date_before(today, k);
        if (day.empty() || s_finals_sent.count(day)) continue;
        s_finals_sent.insert(day);
        post_publish("activity/" + day, activity_doc(*stats, k, day, posix), true);
    }
    if (!s_old_cleared) {
        // The broker keeps the last 14 finished days.
        s_old_cleared = true;
        for (int k = 15; k <= 30; k++) post_publish("activity/" + date_before(today, k), "", true);
    }
}

void build_state()
{
    if (!running() || !app::in_focus()) return;   // pctl stays the system's while in the background
    PctlStatus st;
    pctl_status_fetch(&st);
    PtState pt;
    pctl_play_timer_query(&pt);
    PtSample sample;
    pctl_play_timer_sample(&sample);
    bool accurate = false;
    const bool accurate_ok = R_SUCCEEDED(time_network_accuracy(&accurate));
    SysInfo si;
    sysinfo_get(&si);
    u64 posix = 0;
    LocalTime today{};
    const bool dated = time_local_now(&posix, &today);
    const std::string date = dated ? date_text(today.year, today.month, today.day) : "";
    const SyncRecords rec = sync_files::records_from(config::get());
    const std::string version = app::version();

    std::string last_result;
    if (s_agent) {
        last_result = s_agent_status.link.last_result;
    } else {
        std::lock_guard<std::mutex> lk(s_shared->m);
        last_result = s_shared->status.last_result;
    }
    auto stats = play_data::latest("");
    const bool activity_ok = stats && play_data::fresh("", std::chrono::minutes(15)) && stats->windows_ok;

    SyncSnapshot s{};
    s.source = "app";
    s.ts = posix;
    s.local_date = date.c_str();
    s.weekday = dated ? today.wday : -1;
    s.clock_accurate_ok = accurate_ok;
    s.clock_accurate = accurate;
    s.conf = &s_conf;
    s.sys = &si;
    s.app_version = version.c_str();
    s.agent_version = s_agent ? s_hello.version : nullptr;
    s.read_only = app::read_only();
    s.status = &st;
    s.timer = &pt;
    s.spent_ok = R_SUCCEEDED(sample.session_rc) && R_SUCCEEDED(sample.spent_rc);
    s.spent_ns = sample.spent_ns;
    s.records = &rec;
    s.activity_ok = activity_ok;
    s.activity_s = activity_ok ? (uint32_t)play_data::today_total_s(*stats) : 0;
    s.agent = s_agent;
    s.last_result = last_result.c_str();
    // The profile order's choices (Home Assistant's select).
    char profile_names[16][SYNC_PROFILE_MAX];
    size_t n_profiles = 0;
    for (const auto& p : profiles::list()) {
        if (p.name.size() >= SYNC_PROFILE_MAX || n_profiles == 16) continue;
        std::snprintf(profile_names[n_profiles++], SYNC_PROFILE_MAX, "%s", p.name.c_str());
    }
    s.profiles = profile_names;
    s.n_profiles = n_profiles;

    std::string doc(16384, '\0');
    const size_t n = sync_state_build(&s, &doc[0], doc.size());
    if (!n) return;
    doc.resize(n);
    const std::string body = without_ts(doc);
    if (s_agent) {
        // The agent publishes what it gets at once: a change, or the poll's
        // fresh time stamp.
        const auto now = Clock::now();
        const int every = s_conf.poll_s > 2 ? s_conf.poll_s - 1 : 1;
        if (body == s_state_body && now - s_agent_state_at < std::chrono::seconds(every)) return;
        if (s_read_only_sent != (app::read_only() ? 1 : 0)) {
            // nro_state.txt says it (ui::on_mode_changed): the agent reads it
            // again and its discovery follows.
            s_read_only_sent = app::read_only() ? 1 : 0;
            agent_ok(agent_client::reload(), "reload");
        }
        if (s_agent && agent_ok(agent_client::push(AgentCmd_PushState, doc), "state")) {
            s_state_body = body;
            s_agent_state_at = now;
        }
        return;
    }
    std::lock_guard<std::mutex> lk(s_shared->m);
    s_shared->state = doc;
    s_shared->state_at = Clock::now();
    s_shared->read_only = app::read_only();
    if (s_read_only_sent != (app::read_only() ? 1 : 0)) {
        s_read_only_sent = app::read_only() ? 1 : 0;
        s_shared->discovery_changed = true;
        wake(*s_shared);
    }
    if (posix) {
        s_shared->posix_base = posix;
        s_shared->posix_at = Clock::now();
    }
    if (body != s_state_body) {
        s_state_body = body;
        s_shared->state_changed = true;
        wake(*s_shared);
    }
}

// The saved profiles: the discovery's select and the agent's profiles.txt.
void check_profiles()
{
    const auto list = profiles::list();
    std::vector<std::string> names;
    for (const auto& p : list) names.push_back(p.name);
    if (s_profiles_known && names == s_profiles_sent) return;
    s_profiles_known = true;
    s_profiles_sent = names;
    sync_files::write_profiles(list);
    if (s_agent) {
        agent_ok(agent_client::reload(), "reload");   // its discovery's select
        return;
    }
    if (!s_shared) return;
    std::lock_guard<std::mutex> lk(s_shared->m);
    s_shared->profiles = names;
    s_shared->discovery_changed = true;
    wake(*s_shared);
}

void post_done(uint32_t id, const SyncOutcome& out)
{
    if (s_agent) {
        AgentResult r{};
        r.id = id;
        r.rc = out.rc;
        r.applied = out.applied;
        r.changed = out.changed;
        r.reason = (uint8_t)out.reason;
        r.relock_failed = out.relock_failed;
        agent_ok(agent_client::order_result(r), "result");
        return;
    }
    if (!s_shared) return;
    std::lock_guard<std::mutex> lk(s_shared->m);
    s_shared->done.push_back({ id, out });
    wake(*s_shared);
}

// "Export a diagnostic report" from Home Assistant: saved on the SD card,
// and published when the settings allow it.
SyncOutcome export_report()
{
    SyncOutcome out;
    sync_outcome_init(&out);
    std::string report = diagnostic::current_report();
    std::string error;
    if (diagnostic::save(report, &error).empty()) {
        out.reason = SyncReason_Busy;
        brls::Logger::warning("sync: report not saved: {}", error);
        return out;
    }
    out.applied = true;
    if (s_conf.publish_report) {
        if (report.size() > REPORT_MAX) report.resize(REPORT_MAX);
        post_publish("report", report, true);
    }
    return out;
}

void next_order();

void finish_order(const sync_orders::Order& o, const SyncOutcome& out, uint32_t generation)
{
    // The new state first: the engine publishes it right after the event.
    s_dirty = true;
    build_state();
    // A session that ended meanwhile keeps the order on the broker: the
    // next one gets it again.
    if (generation == s_generation) post_done(o.id, out);
    sync_orders::tell(o, out);
    if (out.applied && out.changed) TabBase::refresh_shown();
    s_order_running = false;
    brls::sync([]() { next_order(); });
}

// The lock screen (Security › Ask for the PIN › To open PlayGuard) is up and
// the policy asks on the console: nobody may answer before the PIN.
bool locked_out()
{
    if (s_conf.policy != SyncPolicy_Ask) return false;
    for (brls::Activity* a : brls::Application::getActivitiesStack())
        if (dynamic_cast<LockActivity*>(a)) return true;
    return false;
}

// One order at a time, only in the foreground (an order may open a dialog
// and reads the console), never behind the lock screen.
void next_order()
{
    if (s_order_running || s_queue.empty() || !app::in_focus() || locked_out()) return;
    const sync_orders::Order o = s_queue.front();
    s_queue.pop_front();
    s_order_running = true;
    const uint32_t generation = s_generation;
    if (o.intent.kind == SyncIntent_ExportReport) {
        finish_order(o, export_report(), generation);
        return;
    }
    sync_orders::run(o, s_conf.policy, s_conf.remote_timer_writes,
                     [o, generation](const SyncOutcome& out) { finish_order(o, out, generation); });
}

void pump()
{
    if (!s_shared) return;
    std::deque<sync_orders::Order> orders;
    bool saved = false, wanted = false;
    SyncConf conf{};
    {
        std::lock_guard<std::mutex> lk(s_shared->m);
        orders.swap(s_shared->orders);
        saved = s_shared->conf_saved;
        conf = s_shared->saved;
        s_shared->conf_saved = false;
        wanted = s_shared->state_wanted;
        s_shared->state_wanted = false;
    }
    if (wanted) s_dirty = true;
    if (saved) {
        // Only what an order may change: Home Assistant's discovery switch.
        s_conf.ha_discovery = conf.ha_discovery;
        std::string error;
        if (!sync_files::save(s_conf, &error)) brls::Logger::warning("sync: sync.conf not saved: {}", error);
    }
    for (auto& o : orders) s_queue.push_back(std::move(o));
    next_order();
}

void apply_conf();

// In agent mode, every second: the foreground, the agent's status, the
// finished days waiting, and the next order.
void agent_tick()
{
    const bool fg = app::in_focus();
    if ((fg ? 1 : 0) != s_agent_fg) {
        if (!agent_ok(agent_client::set_foreground(fg), "foreground")) return;
        s_agent_fg = fg ? 1 : 0;
        if (fg) s_dirty = true;   // back in front: PlayGuard reads the console again
    }
    AgentStatus st;
    if (!agent_ok(agent_client::status(&st), "status")) return;
    s_agent_status = st;
    agent_flush_finals();
    // One at a time, in the foreground (next_order says when).
    if (s_agent && fg && !s_order_running && s_queue.empty()) {
        AgentOrder a;
        if (agent_ok(agent_client::pop_order(&a), "order") && a.id) {
            sync_orders::Order o;
            o.id = a.id;
            o.intent = a.intent;
            o.entity.assign(a.entity, strnlen(a.entity, sizeof(a.entity)));
            o.payload.assign(a.payload, strnlen(a.payload, sizeof(a.payload)));
            o.retained = a.retained;
            brls::Logger::info("sync: order {} from the agent: {}={}", o.id, o.entity, o.payload);
            s_queue.push_back(o);
        }
    }
    next_order();
}

void tick()
{
    const auto now = Clock::now();
    // The agent started or stopped (Tools › Optional modules, a crash): the
    // link follows.
    if (!s_hold && wanted() && !s_agent && now - s_last_probe >= PROBE_EVERY) {
        s_last_probe = now;
        if (agent_client::available() ? !s_agent_refused : !s_shared) apply_conf();
    }
    if (s_agent) agent_tick();
    if (!running()) return;
    pump();
    if (!app::in_focus()) return;
    if (s_dirty || now - s_last_state >= std::chrono::seconds(s_conf.poll_s)) {
        s_dirty = false;
        s_last_state = now;
        build_state();
        check_profiles();
    }
    if (!play_data::busy("") && !play_data::fresh("", std::chrono::duration_cast<std::chrono::seconds>(FETCH_EVERY)) &&
        now - s_last_fetch >= FETCH_EVERY) {
        s_last_fetch = now;
        play_data::fetch(nullptr);
    }
}

void launch()
{
    auto sh = std::make_shared<Shared>();
    sh->conf = s_conf;
    sh->read_only = app::read_only();
    // What the first discovery needs, before the worker connects.
    const auto list = profiles::list();
    s_profiles_sent.clear();
    for (const auto& p : list) s_profiles_sent.push_back(p.name);
    sh->profiles = s_profiles_sent;
    sync_files::write_profiles(list);
    try {
        s_worker = std::thread(worker, sh, s_conf, app::version());
    } catch (const std::system_error& e) {
        brls::Logger::error("sync: no thread for the remote link ({})", e.what());
        return;
    }
    s_shared = sh;
    s_dirty = true;
    s_profiles_known = true;
    s_finals_sent.clear();
    s_old_cleared = false;
    s_names_sent.clear();
    s_week_sent.clear();
    s_state_body.clear();
    s_read_only_sent = app::read_only() ? 1 : 0;
    brls::Logger::info("sync: remote link started ({}:{})", s_conf.host, s_conf.port);
    on_play_data();
}

// Stops the worker: "offline" and DISCONNECT, waited for a few seconds; a
// worker still stuck in a connection after that is left to end on its own.
void halt()
{
    if (!s_shared) return;
    std::shared_ptr<Shared> sh = s_shared;
    s_shared.reset();
    s_generation++;
    std::unique_lock<std::mutex> lk(sh->m);
    sh->stop = true;
    wake(*sh);
    const bool ended = sh->cv.wait_for(lk, EXIT_WAIT, [&]() { return sh->finished; });
    lk.unlock();
    if (s_worker.joinable()) {
        if (ended) s_worker.join();
        else s_worker.detach();
    }
    // Orders not carried out stay retained on the broker: the next session
    // gets them again.
    s_queue.clear();
}

// PlayGuard's session on the agent ends: it reads the console by itself
// again, and answers the orders it handed out "waiting" (they stay on the
// broker).
void agent_leave()
{
    if (!s_agent) return;
    agent_client::close();
    s_agent = false;
    s_generation++;
    s_queue.clear();
    s_agent_finals.clear();
}

void agent_lost()
{
    if (!s_agent) return;
    brls::Logger::warning("sync: the agent went away");
    agent_leave();
    s_last_probe = {};   // the next second: PlayGuard's own session, or the agent again
}

// What the agent did while PlayGuard was closed: its records (extra time
// given, the console lock, a relock pending) and its changes, into the
// history with the time they were made.
void agent_adopt()
{
    // Not when PlayGuard saved after the agent's last change: the agent then
    // reads PlayGuard's (nro_state.txt) at once.
    if (sync_files::agent_records_newer()) {
        AgentRecords r{};
        if (agent_ok(agent_client::records(&r), "records") && r.valid &&
            sync_files::records_into(r.records, config::get())) {
            ui::save_config();
            brls::Logger::info("sync: the agent's records adopted");
        }
    } else {
        agent_ok(agent_client::reload(), "reload");
    }
    const auto events = sync_files::take_agent_events();
    const TimeRule* rule = time_console_rule();
    for (const auto& ev : events) {
        LocalTime t{};
        std::string when;
        if (rule && rule->to_local(rule->ctx, ev.ts, &t))
            when = fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}", (int)t.year, (int)t.month, (int)t.day, (int)t.hour,
                               (int)t.minute);
        sync_orders::import(ev, when);
    }
    if (!events.empty()) brls::Logger::info("sync: {} change(s) of the agent in the history", events.size());
}

// The agent runs: its session is the link, PlayGuard's own stops. True when
// the agent holds the link (PlayGuard's session or not).
bool agent_join()
{
    s_agent_refused = false;
    if (!agent_client::available()) return false;
    AgentHelloReply r{};
    Result rc = 0;
    if (!agent_client::open(&r, &rc)) {
        brls::Logger::warning("sync: the agent did not answer (0x{:08X})", (unsigned)rc);
        return false;
    }
    s_hello = r;
    s_hello.version[sizeof(s_hello.version) - 1] = '\0';
    halt();   // "offline" from PlayGuard's own session first; the agent says "online" again below
    if (!r.accepted) {
        // Another protocol: the agent keeps the link to itself (Tools ›
        // Optional modules updates it).
        s_agent_refused = true;
        brls::Logger::warning("sync: the agent {} speaks protocol {}, PlayGuard {}", s_hello.version, r.protocol,
                              AGENT_PROTOCOL);
        return true;
    }
    s_agent = true;
    s_agent_fg = -1;
    s_agent_status = AgentStatus{};
    s_agent_state_at = {};
    s_agent_activity.clear();
    s_agent_finals.clear();
    s_dirty = true;
    s_finals_sent.clear();
    s_old_cleared = false;
    s_names_sent.clear();
    s_week_sent.clear();
    s_state_body.clear();
    s_read_only_sent = app::read_only() ? 1 : 0;
    s_profiles_known = false;
    brls::Logger::info("sync: the agent {} holds the link", s_hello.version);
    agent_adopt();
    if (s_agent) agent_ok(agent_client::sync_now(), "sync now");
    if (s_agent) on_play_data();
    return true;
}

void apply_conf()
{
    if (!wanted()) {
        halt();
        if (s_agent) agent_ok(agent_client::reload(), "reload");   // the agent stops its link too
        agent_leave();
        s_agent_refused = false;
        return;
    }
    if (s_agent) {
        agent_ok(agent_client::reload(), "reload");   // sync.conf again
        return;
    }
    if (agent_join()) return;
    if (!s_shared) {
        launch();
        return;
    }
    std::lock_guard<std::mutex> lk(s_shared->m);
    s_shared->conf = s_conf;
    s_shared->reconf = true;
    s_shared->read_only = app::read_only();
    wake(*s_shared);
}
}   // namespace

void start()
{
    if (s_started) return;
    s_started = true;
    s_conf = sync_files::load();
    s_timer = new brls::RepeatingTimer();
    s_timer->setCallback([]() { tick(); });
    s_timer->start(1000);
    s_listen = play_data::listen([]() { on_play_data(); });
    apply_conf();
}

void stop()
{
    if (!s_started) return;
    halt();
    agent_leave();
    if (s_timer) {
        s_timer->stop();
        delete s_timer;
        s_timer = nullptr;
    }
    play_data::unlisten(s_listen);
    s_started = false;
}

void reload()
{
    s_conf = sync_files::load();
    s_dirty = true;
    if (s_started) apply_conf();
}

void sync_now()
{
    s_dirty = true;
    s_names_sent.clear();
    s_week_sent.clear();
    s_finals_sent.clear();
    if (s_agent) {
        agent_ok(agent_client::sync_now(), "sync now");
    } else if (s_shared) {
        std::lock_guard<std::mutex> lk(s_shared->m);
        s_shared->sync_now = true;
        wake(*s_shared);
    } else {
        return;
    }
    tick();
    if (!play_data::busy("")) play_data::fetch(nullptr);
}

void changed()
{
    s_dirty = true;
}

void agent_stopping()
{
    if (!s_agent) return;
    if (agent_ok(agent_client::prepare_shutdown(), "shutdown")) {
        // A clean "offline" before the process ends (else the broker's last
        // will says it).
        AgentStatus st;
        for (int i = 0; i < 20 && s_agent; i++) {
            if (!agent_ok(agent_client::status(&st), "status") || st.link.state != SyncLink_Online) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    agent_leave();
}

void hold(bool on)
{
    s_hold = on;
}

void agent_started()
{
    // pg:agent is there once the agent is up, a moment after its start.
    for (int i = 0; i < 30 && !agent_client::available(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    reload();
}

Status status()
{
    Status st;
    st.enabled = s_conf.enabled;
    st.ready = s_conf.enabled && !sync_conf_problem(&s_conf);
    st.running = running();
    st.agent = s_agent;
    st.agent_refused = s_agent_refused;
    if (s_agent || s_agent_refused) st.agent_version = s_hello.version;
    if (s_agent) {
        st.link = s_agent_status.link;
        st.pending = s_agent_status.pending + s_queue.size();
        st.agent_read_only = s_agent_status.read_only;
    } else if (s_shared) {
        std::lock_guard<std::mutex> lk(s_shared->m);
        st.link = s_shared->status;
        st.pending = s_shared->pending + s_queue.size();
    }
    return st;
}

std::vector<std::string> log_lines()
{
    if (s_agent) {
        std::string text;
        std::vector<std::string> lines;
        if (!agent_ok(agent_client::log(&text), "log")) return lines;
        for (size_t at = 0; at < text.size();) {
            size_t end = text.find('\n', at);
            if (end == std::string::npos) end = text.size();
            if (end > at) lines.push_back(text.substr(at, end - at));
            at = end + 1;
        }
        return lines;
    }
    if (!s_shared) return {};
    std::lock_guard<std::mutex> lk(s_shared->m);
    return std::vector<std::string>(s_shared->log.begin(), s_shared->log.end());
}

std::string report_section()
{
    const SyncConf c = s_started ? s_conf : sync_files::load();
    const Status st = status();
    static const char* const STATES[] = { "off", "waiting", "connecting", "online" };
    std::string out = "\n=== Remote link ===\n";
    if (!sync_files::exists()) return out + "not set up\n";
    out += fmt::format("settings    : enabled={} ready={} tls={} mqtt={} user={} anonymous={} policy={} timer_writes={} "
                       "discovery={} report={} activity={} poll={}s\n",
                       c.enabled ? 1 : 0, st.ready ? 1 : 0, c.tls ? 1 : 0, sync_mqtt_version_name(c.mqtt_version),
                       c.username[0] ? "set" : "none",
                       c.allow_anonymous ? 1 : 0, sync_policy_name(c.policy), c.remote_timer_writes ? 1 : 0,
                       c.ha_discovery ? 1 : 0, c.publish_report ? 1 : 0, c.publish_activity ? 1 : 0, c.poll_s);
    if (st.agent || st.agent_refused)
        out += fmt::format("agent       : {} {}\n", st.agent_version,
                           st.agent ? (st.agent_read_only ? "(holds the link, read-only)" : "(holds the link)")
                                    : "(another protocol: not used)");
    if (!st.running) return out + "session     : not running\n";
    const SyncStatus& l = st.link;
    out += fmt::format("session     : {} mqtt={} connects={} publishes={} orders={} rejected={} dropped={} pending={}\n",
                       STATES[l.state <= SyncLink_Online ? l.state : 0],
                       l.protocol == MQTT_V5 ? "5.0" : l.protocol == MQTT_V311 ? "3.1.1" : "-", l.connects, l.publishes,
                       l.orders, l.rejected, l.dropped, st.pending);
    if (l.error[0]) {
        // "cannot find <host>": reports are public, the broker's name is not.
        std::string error = l.error;
        for (size_t at; c.host[0] && (at = error.find(c.host)) != std::string::npos;)
            error.replace(at, std::strlen(c.host), "<broker>");
        out += fmt::format("last error  : {}\n", error);
    }
    if (l.last_result[0]) out += fmt::format("last order  : {}\n", l.last_result);
    return out;
}

}   // namespace sync_flow
