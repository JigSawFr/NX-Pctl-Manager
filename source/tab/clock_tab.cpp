// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/clock_tab.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/ntp_servers.hpp"

using namespace brls::literals;

namespace
{
constexpr auto SNAPSHOT_EVERY = std::chrono::seconds(10);

std::string region_label(const std::string& id)
{
    return brls::getStr("playguard/clock/region_names/" + id);
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
    warning->setSingleLine(false);
    progress_text->setSingleLine(false);

    this->server = clock_flow::current_server();
    this->region_id = find_region(this->server);

    region->registerClickAction([this](brls::View*) { this->choose_region(); return true; });
    server_cell->registerClickAction([this](brls::View*) { this->choose_server(); return true; });
    custom->registerClickAction([this](brls::View*) { this->enter_custom(); return true; });
    measure_cell->registerClickAction([this](brls::View*) { this->measure(); return true; });
    apply_cell->registerClickAction([this](brls::View*) {
        if (!ui::refuse_read_only()) this->apply();
        return true;
    });
    ui::set_visible(progress.getView(), false);
    ui::set_visible(warning.getView(), false);
    // Clocks tick and a measurement expires: keep the screen current.
    this->enable_auto_refresh(1000);
    // Ⓧ reads the clocks again at once (the timer moves them forward between reads).
    this->registerAction("playguard/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        this->snap_ok = false;
        this->refresh();
        return true;
    });
}

void ClockTab::refresh()
{
    const auto now = std::chrono::steady_clock::now();
    if (!this->snap_ok || now - this->snap_at >= SNAPSHOT_EVERY || now < this->snap_at) {
        time_clock_snapshot(&this->snap);
        this->snap_at = now;
        this->snap_ok = true;
    }
    const TimeSnapshot& s = this->snap;
    const uint64_t ticked = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(now - this->snap_at).count();
    const std::string na = "playguard/common/unavailable"_i18n;
    user->setDetailText(R_SUCCEEDED(s.user_rc) ? ui::time_text(s.user_time + ticked) : na);
    network->setDetailText(R_SUCCEEDED(s.network_rc) ? ui::time_text(s.network_time + ticked) : na);
    accuracy->setDetailText(ui::bool_text(R_SUCCEEDED(s.accuracy_rc), s.accuracy,
                                          "playguard/common/yes"_i18n, "playguard/common/no"_i18n));
    accuracy->setDetailTextColor(R_SUCCEEDED(s.accuracy_rc) ? (s.accuracy ? ui::color_ok() : ui::color_warn())
                                                            : ui::color_neutral());
    autosync->setDetailText(ui::bool_text(R_SUCCEEDED(s.automatic_rc), s.automatic,
                                          "playguard/common/on"_i18n, "playguard/common/off"_i18n));
    this->autosync_off = R_SUCCEEDED(s.automatic_rc) && !s.automatic;
    autosync->setDetailTextColor(this->autosync_off ? ui::color_warn() : ui::color_neutral());
    zone->setDetailText(R_SUCCEEDED(s.location_rc) && s.location[0] ? std::string(s.location) : na);

    region->setDetailText(region_label(this->region_id));
    server_cell->setDetailText(this->server);

    // "Set the network clock" only exists while a fresh measurement does.
    if (this->last.ok) {
        const int64_t left = clock_flow::seconds_left(this->last);
        if (left <= 0) {
            this->last = clock_flow::Measurement{};
            this->result->setText("playguard/clock/expired"_i18n);
            ui::set_visible(warning.getView(), false);
        } else {
            // Counts down every second (the tab refreshes itself each second).
            // "1 min 45 s": "1:45" read like the hours:minutes typed elsewhere.
            const std::string span = left >= 60 ? brls::getStr("playguard/common/min_sec", (int)(left / 60), (int)(left % 60))
                                                : brls::getStr("playguard/common/sec", (int)left);
            apply_cell->setDetailText(brls::getStr("playguard/clock/valid_for", span));
        }
    }
    // Measuring only reads: also in read-only mode, where setting is greyed.
    ui::set_visible(apply_cell.getView(), this->last.ok);
    ui::show_writable(apply_cell, !app::read_only());
}

void ClockTab::set_server(const std::string& host, const std::string& reg)
{
    this->server = host;
    this->region_id = reg;
    this->last = clock_flow::Measurement{};
    this->result->setText("");
    ui::set_visible(warning.getView(), false);
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
    this->last = clock_flow::Measurement{};
    ui::set_visible(apply_cell.getView(), false);
    ui::set_visible(warning.getView(), false);
    this->result->setText("");
    // A spinner says it is working; the text says with whom.
    progress_text->setText(brls::getStr("playguard/clock/measuring", this->server));
    ui::set_visible(progress.getView(), true);

    std::weak_ptr<bool> weak = this->alive;
    clock_flow::measure(this->server, [this, weak](const clock_flow::Measurement& m) {
        if (weak.expired()) return;   // the tab was closed meanwhile
        this->busy = false;
        ui::set_visible(progress.getView(), false);
        this->last = m;
        this->result->setText(m.report);
        // The servers disagreeing, and a write that would be refused, in amber.
        std::string warn = m.warning;
        if (m.ok && this->autosync_off) warn += (warn.empty() ? "" : "\n") + "playguard/clock/autosync_off_note"_i18n;
        warning->setText(warn);
        ui::set_visible(warning.getView(), !warn.empty());
        this->refresh();   // shows "Set the network clock"
    });
}

void ClockTab::apply()
{
    clock_flow::apply(this->last, [this](const std::string& message) {
        this->last = clock_flow::Measurement{};
        this->result->setText(message);
        ui::set_visible(warning.getView(), false);
        this->snap_ok = false;   // the network clock just changed: read it again
        this->refresh();
    });
}

brls::View* ClockTab::create()
{
    return new ClockTab();
}
