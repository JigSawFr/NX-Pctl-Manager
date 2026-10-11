// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/clock_flow.hpp"

#include <algorithm>
#include <atomic>
#include <borealis.hpp>
#include <fmt/format.h>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include "action/history_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/ntp_client.hpp"
#include "util/ntp_servers.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace clock_flow
{

namespace
{
std::string signed_seconds(int64_t s)
{
    return brls::getStr("playguard/clock/seconds", (s > 0 ? "+" : "") + std::to_string(s));
}

std::string ntp_error_text(const ntp::Reply& r)
{
    switch (r.kind) {
        case ntp::Error::BadHost:  return "playguard/clock/ntp_err/bad_host"_i18n;
        case ntp::Error::Lookup:   return "playguard/clock/ntp_err/lookup"_i18n;
        case ntp::Error::Timeout:  return "playguard/clock/ntp_err/timeout"_i18n;
        case ntp::Error::BadReply: return "playguard/clock/ntp_err/bad_reply"_i18n;
        default:                   return "playguard/clock/ntp_err/network"_i18n;
    }
}

int64_t elapsed_s(const Measurement& m)
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m.at).count();
}

// After the confirmation: the write itself, with its reports.
void write_clock(const Measurement& m, const std::string& before, std::function<void(const std::string&)> done)
{
    if (seconds_left(m) <= 0) {
        done("playguard/clock/expired"_i18n);
        return;
    }
    const uint64_t target = projected_now(m);
    TimeApply a;
    time_clock_apply(target, &a);

    std::string message;
    if (a.refused_automatic) message = "playguard/clock/applied_refused"_i18n;
    else if (!a.write_attempted) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.open_rc);
    else if (R_FAILED(a.write_rc)) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.write_rc);
    else if (!a.verified) message = "playguard/clock/applied_err"_i18n + " — " + ui::rc_text(a.verify_rc);
    else if (R_FAILED(a.after.accuracy_rc) || !a.after.accuracy) message = "playguard/clock/applied_pending"_i18n;
    else message = "playguard/clock/applied_ok"_i18n;
    if (a.write_attempted && R_SUCCEEDED(a.write_rc)) history_flow::record_event("clock", "", ui::time_text(target));

    diagnostic::save(fmt::format(
        "=== Network clock change ===\nserver={}\ntarget_utc={}\nopen_rc=0x{:08X} write_attempted={} write_rc=0x{:08X}\n"
        "verify_attempted={} verify_rc=0x{:08X} verified={} readback={}\nrefused_automatic={}\n\n--- Before ---\n{}\n--- After ---\n{}",
        m.server, target, (unsigned)a.open_rc, a.write_attempted, (unsigned)a.write_rc,
        a.verify_attempted, (unsigned)a.verify_rc, a.verified, a.readback, a.refused_automatic,
        before, diagnostic::current_report()));
    done(message);
}

// A spinner and what is being asked; Cancel (or B) sets `cancelled`.
brls::Dialog* progress_dialog(const std::string& text, std::shared_ptr<std::atomic<bool>> cancelled)
{
    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->setWidth(60);
    spinner->setHeight(60);
    spinner->setMarginBottom(24);
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(22);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(40, 40, 40, 40);
    box->addView(spinner);
    box->addView(label);
    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/cancel"_i18n, [cancelled]() { *cancelled = true; });
    ui::on_cancel(dialog, [cancelled]() { *cancelled = true; });
    return dialog;
}
}   // namespace

std::string current_server()
{
    const auto& cfg = config::get();
    return cfg.ntp_server.empty() ? ntp::default_server_for_console() : cfg.ntp_server;
}

void measure(const std::string& server, std::function<void(const Measurement&)> done)
{
    std::vector<std::string> hosts = { server };
    for (auto& h : ntp::cross_check_servers(server)) hosts.push_back(h);
    auto run = [hosts, done]() {
        // Query the servers in parallel so an unreachable one costs a single
        // timeout (one after the other when no thread can be had).
        std::vector<ntp::Reply> replies(hosts.size());
        // Each stops waiting when the app quits: it waits for this thread.
        std::vector<std::thread> workers;
        for (size_t i = 0; i < hosts.size(); i++) {
            try {
                workers.emplace_back([&replies, &hosts, i]() { replies[i] = ntp::fetch(hosts[i], 2500, 2, ui::quitting); });
            } catch (const std::system_error& e) {
                brls::Logger::warning("NTP: no thread for {} ({}), querying it here", hosts[i], e.what());
                replies[i] = ntp::fetch(hosts[i], 2500, 2, ui::quitting);
            }
        }
        for (auto& w : workers) w.join();
        if (ui::quitting()) return;
        brls::sync([hosts, replies, done]() {
            // Bring every sample to the same instant, then take the median.
            const auto ref = std::chrono::steady_clock::now();
            std::vector<int64_t> estimates;
            for (const auto& r : replies)
                if (r.ok)
                    estimates.push_back((int64_t)r.unix_seconds +
                        std::chrono::duration_cast<std::chrono::seconds>(ref - r.received_at).count());

            TimeSnapshot s;
            time_clock_snapshot(&s);
            Measurement m;
            m.server = hosts.front();
            m.at     = ref;
            for (size_t i = 0; i < hosts.size(); i++) {
                const auto& r = replies[i];
                if (r.ok) {
                    const int64_t now_est = (int64_t)r.unix_seconds +
                        std::chrono::duration_cast<std::chrono::seconds>(ref - r.received_at).count();
                    const std::string diff = R_SUCCEEDED(s.network_rc) ? signed_seconds(now_est - (int64_t)s.network_time) : "?";
                    m.report += brls::getStr("playguard/clock/result_line_ok", hosts[i], ui::time_text((uint64_t)now_est), diff);
                } else {
                    m.report += brls::getStr("playguard/clock/result_line_err", hosts[i], ntp_error_text(r));
                    brls::Logger::info("NTP {}: {}", hosts[i], r.error);
                }
                m.report += "\n";
            }
            if (estimates.empty()) {
                m.report += "playguard/clock/result_none"_i18n;
                done(m);
                return;
            }
            std::sort(estimates.begin(), estimates.end());
            m.ok           = true;
            // Median; an even count takes the mean of the two middle samples
            // (the difference as unsigned: hi >= lo, so it cannot overflow).
            const size_t mid = estimates.size() / 2;
            int64_t median   = estimates[mid];
            if (estimates.size() % 2 == 0) {
                const int64_t lo = estimates[mid - 1];
                median = lo + (int64_t)(((uint64_t)estimates[mid] - (uint64_t)lo) / 2);
            }
            m.unix_seconds = (uint64_t)median;
            m.spread       = estimates.back() - estimates.front();
            const std::string diff = R_SUCCEEDED(s.network_rc) ? signed_seconds((int64_t)m.unix_seconds - (int64_t)s.network_time) : "?";
            m.report += brls::getStr("playguard/clock/result_summary", ui::time_text(m.unix_seconds), diff);
            if (m.spread > 5) m.warning = brls::getStr("playguard/clock/result_spread", (int)m.spread);
            done(m);
        });
    };
    // Its spinner is on screen: not behind the play log or the game icons.
    ui::in_background("NTP", run);
}

int64_t seconds_left(const Measurement& m)
{
    if (!m.ok) return 0;
    const int64_t e = elapsed_s(m);
    return e < 0 ? 0 : LIFETIME_S - e;
}

uint64_t projected_now(const Measurement& m)
{
    const int64_t e = elapsed_s(m);
    return m.unix_seconds + (uint64_t)(e < 0 ? 0 : e);
}

void apply(const Measurement& m, std::function<void(const std::string& message)> done)
{
    TimeSnapshot s;
    time_clock_snapshot(&s);
    // Setting the clock would be refused: say so now, not after the confirmation.
    if (R_SUCCEEDED(s.automatic_rc) && !s.automatic) {
        ui::info(ui::rc_text(NXM_RC_AUTOCORRECT_OFF));
        return;
    }
    if (!m.ok) {
        ui::info("playguard/clock/need_measure"_i18n);
        return;
    }
    if (seconds_left(m) <= 0) {
        ui::info("playguard/clock/expired"_i18n);
        return;
    }
    const uint64_t preview = projected_now(m);
    char utc[48];
    time_format_utc(preview, utc, sizeof(utc));
    std::string body = brls::getStr("playguard/clock/confirm_apply", ui::time_text(preview), std::string(utc));
    if (m.spread > 5) body += brls::getStr("playguard/clock/confirm_apply_spread", (int)m.spread);
    // Measured on a console (docs/parental-controls.md): the play timer
    // starts the day over when the clock changes. Say so before, not after.
    body += "playguard/clock/confirm_apply_resets"_i18n;

    auto finish = [done](const std::string& message) {
        if (done) done(message);
        ui::info(message);
    };
    ui::confirm(body, "playguard/clock/apply_confirm"_i18n, [m, finish]() {
        // A "before" report first, so the change can be analysed. When the SD
        // card refuses it (full, read-only), ask rather than give up.
        const std::string before = diagnostic::current_report();
        std::string err;
        if (diagnostic::save("=== Before network clock change ===\n" + before, &err).empty()) {
            brls::sync([m, before, err, finish]() {
                ui::confirm(brls::getStr("playguard/clock/report_err_ask", err), "playguard/clock/apply_confirm"_i18n,
                            [m, before, finish]() { write_clock(m, before, finish); });
            });
            return;
        }
        write_clock(m, before, finish);
    });
}

void guided(std::function<void()> done)
{
    if (app::read_only()) {
        ui::notify(ui::rc_text(NXM_RC_READ_ONLY));
        return;
    }
    const std::string server = current_server();
    auto* ask = ui::dialog(brls::getStr("playguard/clock/guided_body", server));
    ask->addButton("hints/cancel"_i18n, []() {});
    ask->addButton("playguard/clock/guided_measure"_i18n, [server, done]() {
        brls::sync([server, done]() {
            auto cancelled = std::make_shared<std::atomic<bool>>(false);
            auto* progress = progress_dialog(brls::getStr("playguard/clock/measuring", server), cancelled);
            progress->open();
            measure(server, [progress, cancelled, done](const Measurement& m) {
                if (*cancelled) return;   // the dialog is already closed
                *cancelled = true;
                progress->close([m, done]() {
                    brls::sync([m, done]() {
                        if (!m.ok) {
                            ui::info(m.report);
                            return;
                        }
                        apply(m, [done](const std::string&) { if (done) done(); });
                    });
                });
            });
        });
    });
    ask->setCancelable(true);
    ask->open();
}

}   // namespace clock_flow
