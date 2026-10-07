// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/clock_check.hpp"

#include <borealis.hpp>
#include <chrono>
#include <cstdlib>
#include <string>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/ntp_client.hpp"
#include "util/ntp_servers.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace clock_check
{

void at_start()
{
    const auto& cfg = config::get();
    if (!cfg.clock_check_at_start) return;
    const std::string server = cfg.ntp_server.empty() ? ntp::default_server_for_console() : cfg.ntp_server;
    brls::async([server]() {
        const ntp::Reply reply = ntp::fetch(server);
        TimeSnapshot clocks;
        time_clock_snapshot(&clocks);
        // The server's time at the moment the clocks were read.
        const auto since = std::chrono::steady_clock::now() - reply.received_at;
        const long long server_now =
            (long long)reply.unix_seconds + std::chrono::duration_cast<std::chrono::seconds>(since).count();
        brls::sync([server, reply, clocks, server_now]() {
            // No network, no answer: nothing to say at start-up.
            if (!reply.ok || R_FAILED(clocks.network_rc)) {
                brls::Logger::info("clock check at start-up: {}", reply.ok ? "network clock unreadable" : reply.error);
                return;
            }
            const long long off = std::llabs(server_now - (long long)clocks.network_time);
            if (off < 60) return;
            ui::notify(brls::getStr("playguard/clock/start_off", ui::fmt_play_time((uint64_t)off), server));
        });
    });
}

}   // namespace clock_check
