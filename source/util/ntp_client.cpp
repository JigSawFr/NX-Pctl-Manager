// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor. GPLv3-or-later (see LICENSE).
#include "ntp_client.hpp"
#include "ntp_packet.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "core/platform.h"

namespace ntp {
namespace {
constexpr int POLL_SLICE_MS = 100;

class Socket {
public:
    explicit Socket(int descriptor) : descriptor_(descriptor) {}
    ~Socket() { if (descriptor_ >= 0) ::close(descriptor_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int get() const { return descriptor_; }
private:
    int descriptor_;
};

std::string socket_error(const char* operation)
{
    const int error_number = errno;
    return std::string(operation) + ": " + std::strerror(error_number);
}

bool make_cookie(std::uint8_t cookie[NTP_COOKIE_SIZE], std::string& error)
{
    if (!platform_random(cookie, NTP_COOKIE_SIZE)) {
        error = "Could not generate NTP request cookie";
        return false;
    }
    bool nonzero = false;
    for (unsigned i = 0; i < NTP_COOKIE_SIZE; ++i) nonzero |= cookie[i] != 0;
    if (!nonzero) cookie[0] = 1;
    return true;
}
}

Reply fetch(const std::string& host, int timeout_ms, unsigned max_addresses, bool (*stop)())
{
    Reply reply;
    const auto stopped = [&reply, stop]() {
        if (!stop || !stop()) return false;
        reply.kind = Error::Timeout;
        reply.error = "Stopped: PlayGuard is quitting";
        return true;
    };
    if (host.empty() || host.find('\0') != std::string::npos) {
        reply.kind = Error::BadHost;
        reply.error = "Enter an NTP hostname or IP address.";
        return reply;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* resolved = nullptr;
    if (stopped()) return reply;
    const int resolve_result = ::getaddrinfo(host.c_str(), "123", &hints, &resolved);
    // Own a non-null list even on a resolver error, so all paths release it.
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(resolved, &::freeaddrinfo);
    if (resolve_result != 0) {
        reply.kind = Error::Lookup;
        reply.error = std::string("NTP address lookup failed: ") + ::gai_strerror(resolve_result);
        return reply;
    }

    reply.kind = Error::Lookup;
    reply.error = "No usable NTP address was returned.";
    unsigned tried = 0;
    for (const addrinfo* address = addresses.get(); address != nullptr && tried < max_addresses;
         address = address->ai_next, ++tried) {
        if (stopped()) return reply;
        Socket socket(::socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (socket.get() < 0) {
            reply.kind = Error::Network;
            reply.error = socket_error("Could not open NTP socket");
            continue;
        }
        const timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        if (::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            reply.kind = Error::Network;
            reply.error = socket_error("Could not set NTP receive timeout");
            continue;
        }
        // Connected UDP restricts received datagrams to this server and port.
        if (::connect(socket.get(), address->ai_addr, address->ai_addrlen) < 0) {
            reply.kind = Error::Network;
            reply.error = socket_error("Could not connect to NTP server");
            continue;
        }

        std::uint8_t cookie[NTP_COOKIE_SIZE];
        if (!make_cookie(cookie, reply.error)) {
            reply.kind = Error::Network;
            return reply;
        }
        std::uint8_t request[NTP_PACKET_SIZE];
        ntp_packet_make_request(request, cookie);
        const ssize_t sent = ::send(socket.get(), request, sizeof(request), 0);
        if (sent != static_cast<ssize_t>(sizeof(request))) {
            reply.kind = Error::Network;
            reply.error = sent < 0 ? socket_error("Could not send NTP request")
                                   : "NTP request was not sent completely.";
            continue;
        }

        // Waited for in short slices, so that quitting does not wait for the
        // whole timeout (the app gives its threads 3 s at exit).
        std::uint8_t response[512];
        ssize_t received = -1;
        errno = ETIMEDOUT;
        for (int waited = 0; waited < timeout_ms;) {
            if (stopped()) return reply;
            pollfd ready{ socket.get(), POLLIN, 0 };
            const int slice = std::min(POLL_SLICE_MS, timeout_ms - waited);
            const int n = ::poll(&ready, 1, slice);
            if (n < 0) break;
            if (n == 0) {
                waited += slice;
                errno = ETIMEDOUT;
                continue;
            }
            received = ::recv(socket.get(), response, sizeof(response), 0);
            break;
        }
        const auto received_at = std::chrono::steady_clock::now();
        if (received < 0) {
            reply.kind = Error::Timeout;
            reply.error = socket_error("No NTP response before the timeout");
            continue;
        }
        std::uint64_t seconds = 0;
        const NtpPacketResult result = ntp_packet_parse(response, static_cast<std::size_t>(received), cookie, &seconds);
        if (result != NTP_PACKET_OK) {
            reply.kind = Error::BadReply;
            reply.error = ntp_packet_error(result);
            continue;
        }
        reply.ok = true;
        reply.kind = Error::None;
        reply.unix_seconds = seconds;
        reply.received_at = received_at;
        reply.error.clear();
        return reply;
    }
    return reply;
}
}
