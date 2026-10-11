// ntp_client — one network time sample over UDP (SNTP), read-only. Adapted
// from anbingxi/NX-Pctl-Manager (diag/fw22-5-readonly).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor. GPLv3-or-later (see LICENSE).
#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace ntp {
// What went wrong, for the user (the English `error` is for the logs).
enum class Error { None, BadHost, Lookup, Network, Timeout, BadReply };

struct Reply {
    bool ok = false;
    std::uint64_t unix_seconds = 0;
    Error kind = Error::None;
    std::string error;
    std::chrono::steady_clock::time_point received_at;
};

// Reads a time sample over connected UDP port 123. Socket services must already
// be initialized by the application. This function does not change system time.
// Tries at most `max_addresses` resolved addresses, waiting `timeout_ms` each,
// and gives up within 0.1 s once `stop` (if any) returns true; the address
// lookup itself cannot be cut short.
Reply fetch(const std::string& host, int timeout_ms = 2500, unsigned max_addresses = 2, bool (*stop)() = nullptr);
}
