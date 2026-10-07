// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/clock_tab.hpp"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <thread>
#include <vector>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/ntp_client.hpp"
#include "util/ntp_servers.hpp"

using namespace brls::literals;

namespace
{
constexpr int64_t SAMPLE_LIFETIME_S = 120;

std::string region_label(const std::string& id)
{
    return brls::getStr("playguard/clock/region_names/" + id);
}

std::string signed_seconds(int64_t s)
{
    return brls::getStr("playguard/clock/seconds", (s > 0 ? "+" : "") + std::to_string(s));
}

std::string find_region(const std::string& host)
{
    for (const auto& r : ntp::regions())
        for (const char* h : r.hosts)
            if (host == h) return r.id;
    return "custom";
}

bool plausible_host(const std::string& h)
{
    if (h.empty() || h.size() > 253) return false;
    for (char c : h)
        if (!(std::isalnum((unsigned char)c) || c == '.' || c == '-' || c == ':' || c == '_')) return false;
    return h.find("://") == std::string::npos;
}
}   // namespace

ClockTab::ClockTab()
    : TabBase("xml/tab/clock.xml")
{
    why->setSingleLine(false);
    result->setSingleLine(false);

    const auto& cfg = config::get();
    this->server = cfg.ntp_server.empty() ? ntp::default_server_for_console() : cfg.ntp_server;
    this->region_id = find_region(this->server);

    region->registerClickAction([this](brls::View*) { this->choose_region(); return true; });
    server_cell->registerClickAction([this](brls::View*) { this->choose_server(); return true; });
    custom->registerClickAction([this](brls::View*) { this->enter_custom(); return true; });
    measure_cell->registerClickAction([this](brls::View*) { this->measure(); return true; });
    apply_cell->registerClickAction([this](brls::View*) { this->apply(); return true; });
    // Clocks tick and a measurement expires: keep the screen current.
    this->enable_auto_refresh(1000);
}

void ClockTab::refresh()
{
    TimeSnapshot s;
    time_clock_snapshot(&s);
    const std::string na = "playguard/common/unavailable"_i18n;
    user->setDetailText(R_SUCCEEDED(s.user_rc) ? ui::time_text(s.user_time) : na);
    network->setDetailText(R_SUCCEEDED(s.network_rc) ? ui::time_text(s.network_time) : na);
    accuracy->setDetailText(ui::bool_text(R_SUCCEEDED(s.accuracy_rc), s.accuracy,
                                          "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    accuracy->setDetailTextColor(R_SUCCEEDED(s.accuracy_rc) ? (s.accuracy ? ui::color_ok() : ui::color_warn())
                                                            : ui::color_neutral());
    autosync->setDetailText(ui::bool_text(R_SUCCEEDED(s.automatic_rc), s.automatic,
                                          "playguard/common/on"_i18n, "playguard/common/off"_i18n));
    autosync->setDetailTextColor(R_SUCCEEDED(s.automatic_rc) && !s.automatic ? ui::color_warn() : ui::color_neutral());
    zone->setDetailText(R_SUCCEEDED(s.location_rc) && s.location[0] ? std::string(s.location) : na);

    region->setDetailText(region_label(this->region_id));
    server_cell->setDetailText(this->server);

    // "Set the network clock" only exists while a fresh measurement does.
    if (this->last.ok) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->last.at).count();
        if (elapsed < 0 || elapsed > SAMPLE_LIFETIME_S) {
            this->last = Measurement{};
            this->result->setText("playguard/clock/expired"_i18n);
        } else {
            // Counts down every second (the tab refreshes itself each second).
            const int64_t left = SAMPLE_LIFETIME_S - elapsed;
            apply_cell->setDetailText(brls::getStr("playguard/clock/valid_for",
                                                   fmt::format("{}:{:02d}", (int)(left / 60), (int)(left % 60))));
        }
    }
    ui::set_visible(apply_cell.getView(), !app::read_only() && this->last.ok);
}

void ClockTab::set_server(const std::string& host, const std::string& reg)
{
    this->server = host;
    this->region_id = reg;
    this->last = Measurement{};
    this->result->setText("");
    auto& cfg = config::get();
    cfg.ntp_server = host;
    ui::save_config();
    this->refresh();
}

void ClockTab::choose_region()
{
    std::vector<std::string> ids, labels;
    for (const auto& r : ntp::regions()) { ids.push_back(r.id); labels.push_back(region_label(r.id)); }
    if (!config::get().custom_servers.empty()) { ids.push_back("custom"); labels.push_back(region_label("custom")); }
    int selected = (int)(std::find(ids.begin(), ids.end(), this->region_id) - ids.begin());
    if (selected >= (int)ids.size()) selected = 0;
    ui::pick("playguard/clock/region"_i18n, labels, selected, [this, ids](int index) {
        const std::string id = ids[index];
        if (id == "custom") {
            this->set_server(config::get().custom_servers.front(), id);
            return;
        }
        for (const auto& r : ntp::regions())
            if (id == r.id) this->set_server(r.hosts.front(), id);
    });
}

void ClockTab::choose_server()
{
    std::vector<std::string> hosts;
    if (this->region_id == "custom") hosts = config::get().custom_servers;
    else
        for (const auto& r : ntp::regions())
            if (this->region_id == r.id) hosts.assign(r.hosts.begin(), r.hosts.end());
    if (hosts.empty()) return;
    int selected = (int)(std::find(hosts.begin(), hosts.end(), this->server) - hosts.begin());
    if (selected >= (int)hosts.size()) selected = 0;
    ui::pick("playguard/clock/server"_i18n, hosts, selected, [this, hosts](int index) {
        this->set_server(hosts[index], this->region_id);
    });
}

void ClockTab::enter_custom()
{
    ui::prompt_text("playguard/clock/custom_header"_i18n, this->server, 253, [this](std::string host) {
        if (!plausible_host(host)) {
            ui::notify(ui::rc_text(NXM_RC_INVALID_ARGUMENT));
            return;
        }
        auto& list = config::get().custom_servers;
        list.erase(std::remove(list.begin(), list.end(), host), list.end());
        list.insert(list.begin(), host);
        if (list.size() > 10) list.resize(10);
        this->set_server(host, find_region(host));
    });
}

void ClockTab::measure()
{
    if (this->busy) {
        ui::notify("playguard/clock/busy"_i18n);
        return;
    }
    this->busy = true;
    this->last = Measurement{};
    ui::set_visible(apply_cell.getView(), false);
    std::vector<std::string> hosts = { this->server };
    for (auto& h : ntp::cross_check_servers(this->server)) hosts.push_back(h);
    this->result->setText(brls::getStr("playguard/clock/measuring", this->server));

    std::weak_ptr<bool> weak = this->alive;
    brls::async([this, weak, hosts]() {
        // Query the servers in parallel so an unreachable one costs a single timeout.
        std::vector<ntp::Reply> replies(hosts.size());
        std::vector<std::thread> workers;
        for (size_t i = 0; i < hosts.size(); i++)
            workers.emplace_back([&replies, &hosts, i]() { replies[i] = ntp::fetch(hosts[i]); });
        for (auto& w : workers) w.join();
        brls::sync([this, weak, hosts, replies]() {
            if (weak.expired()) return;   // the tab was closed meanwhile
            this->busy = false;

            // Bring every sample to the same instant, then take the median.
            auto ref = std::chrono::steady_clock::now();
            std::vector<int64_t> estimates;
            for (const auto& r : replies)
                if (r.ok)
                    estimates.push_back((int64_t)r.unix_seconds +
                        std::chrono::duration_cast<std::chrono::seconds>(ref - r.received_at).count());

            TimeSnapshot s;
            time_clock_snapshot(&s);
            std::string text;
            for (size_t i = 0; i < hosts.size(); i++) {
                const auto& r = replies[i];
                if (r.ok) {
                    int64_t now_est = (int64_t)r.unix_seconds +
                        std::chrono::duration_cast<std::chrono::seconds>(ref - r.received_at).count();
                    std::string diff = R_SUCCEEDED(s.network_rc) ? signed_seconds(now_est - (int64_t)s.network_time) : "?";
                    text += brls::getStr("playguard/clock/result_line_ok", hosts[i], ui::time_text((uint64_t)now_est), diff);
                } else {
                    text += brls::getStr("playguard/clock/result_line_err", hosts[i], r.error);
                }
                text += "\n";
            }
            if (estimates.empty()) {
                this->result->setText(text + "playguard/clock/result_none"_i18n);
                return;
            }
            std::sort(estimates.begin(), estimates.end());
            Measurement m;
            m.ok = true;
            m.unix_seconds = (uint64_t)estimates[estimates.size() / 2];
            m.at = ref;
            m.spread = estimates.back() - estimates.front();
            m.server = hosts.front();
            this->last = m;
            std::string diff = R_SUCCEEDED(s.network_rc) ? signed_seconds((int64_t)m.unix_seconds - (int64_t)s.network_time) : "?";
            text += brls::getStr("playguard/clock/result_summary", ui::time_text(m.unix_seconds), diff);
            if (m.spread > 5) text += "\n" + brls::getStr("playguard/clock/result_spread", (int)m.spread);
            this->result->setText(text);
            this->refresh();   // shows "Set the network clock"
        });
    });
}

void ClockTab::apply()
{
    if (!this->last.ok) {
        ui::info("playguard/clock/need_measure"_i18n);
        return;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->last.at).count();
    if (elapsed < 0 || elapsed > SAMPLE_LIFETIME_S) {
        this->last = Measurement{};
        ui::info("playguard/clock/expired"_i18n);
        return;
    }
    uint64_t preview = this->last.unix_seconds + (uint64_t)elapsed;
    char utc[48];
    time_format_utc(preview, utc, sizeof(utc));
    std::string body = brls::getStr("playguard/clock/confirm_apply", ui::time_text(preview), std::string(utc));
    if (this->last.spread > 5) body += brls::getStr("playguard/clock/confirm_apply_spread", (int)this->last.spread);

    ui::confirm(body, "playguard/clock/apply_confirm"_i18n, [this]() {
        // A "before" report first, so the change can be analysed. When the SD
        // card refuses it (full, read-only), ask rather than give up.
        std::string before = diagnostic::current_report();
        std::string err;
        if (diagnostic::save("=== Before network clock change ===\n" + before, &err).empty()) {
            ui::confirm(brls::getStr("playguard/clock/report_err_ask", err), "playguard/clock/apply_confirm"_i18n,
                        [this, before]() { this->write_clock(before); });
            return;
        }
        this->write_clock(before);
    });
}

void ClockTab::write_clock(const std::string& before)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->last.at).count();
    if (elapsed < 0 || elapsed > SAMPLE_LIFETIME_S) {
        this->last = Measurement{};
        ui::info("playguard/clock/expired"_i18n);
        return;
    }
    const uint64_t target = this->last.unix_seconds + (uint64_t)elapsed;
    TimeApply a;
    time_clock_apply(target, &a);

    std::string message;
    if (a.refused_automatic) message = "playguard/clock/applied_refused"_i18n;
    else if (!a.write_attempted) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.open_rc);
    else if (R_FAILED(a.write_rc)) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.write_rc);
    else if (!a.verified) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.verify_rc);
    else if (R_FAILED(a.after.accuracy_rc) || !a.after.accuracy) message = "playguard/clock/applied_pending"_i18n;
    else message = "playguard/clock/applied_ok"_i18n;

    diagnostic::save(fmt::format(
        "=== Network clock change ===\nserver={}\ntarget_utc={}\nopen_rc=0x{:08X} write_attempted={} write_rc=0x{:08X}\n"
        "verify_attempted={} verify_rc=0x{:08X} verified={} readback={}\nrefused_automatic={}\n\n--- Before ---\n{}\n--- After ---\n{}",
        this->last.server, target, (unsigned)a.open_rc, a.write_attempted, (unsigned)a.write_rc,
        a.verify_attempted, (unsigned)a.verify_rc, a.verified, a.readback, a.refused_automatic,
        before, diagnostic::current_report()));

    this->last = Measurement{};
    this->result->setText(message);
    this->refresh();
    ui::info(message);
}

brls::View* ClockTab::create()
{
    return new ClockTab();
}
