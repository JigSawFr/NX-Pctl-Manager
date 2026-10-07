// Copyright (C) 2026 Taylor. GPLv3-or-later (see LICENSE).
#include "ntp_packet.h"
#include <string.h>

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

void ntp_packet_make_request(uint8_t request[NTP_PACKET_SIZE],
                             const uint8_t cookie[NTP_COOKIE_SIZE])
{
    memset(request, 0, NTP_PACKET_SIZE);
    request[0] = (4u << 3) | 3u; // LI=0, NTPv4, client mode.
    memcpy(request + 40, cookie, NTP_COOKIE_SIZE);
}

NtpPacketResult ntp_packet_parse(const uint8_t *packet, size_t length,
                                 const uint8_t cookie[NTP_COOKIE_SIZE],
                                 uint64_t *unix_seconds)
{
    if (unix_seconds != NULL) *unix_seconds = 0;
    if (packet == NULL || cookie == NULL || unix_seconds == NULL)
        return NTP_PACKET_BAD_ARGUMENT;
    if (length < NTP_PACKET_SIZE) return NTP_PACKET_TOO_SHORT;
    if ((packet[0] >> 6) == 3) return NTP_PACKET_UNSYNCHRONIZED;
    const unsigned version = (packet[0] >> 3) & 7u;
    if (version != 3 && version != 4) return NTP_PACKET_BAD_VERSION;
    if ((packet[0] & 7u) != 4) return NTP_PACKET_BAD_MODE;
    if (packet[1] < 1 || packet[1] > 15) return NTP_PACKET_BAD_STRATUM;
    if (memcmp(packet + 24, cookie, NTP_COOKIE_SIZE) != 0)
        return NTP_PACKET_WRONG_ORIGIN;

    const uint32_t seconds = read_be32(packet + 40);
    const uint32_t fraction = read_be32(packet + 44);
    if (seconds == 0 && fraction == 0) return NTP_PACKET_ZERO_TIMESTAMP;

    // The bounded date interval selects one NTP era without trusting the local
    // clock. It is narrower than one era (2^32 seconds), so cannot be ambiguous.
    const uint64_t epoch_offset = UINT64_C(2208988800);
    const uint64_t earliest = UINT64_C(1577836800); // 2020-01-01 UTC, inclusive.
    const uint64_t latest = UINT64_C(4102444800);   // 2100-01-01 UTC, exclusive.
    for (unsigned era = 0; era < 2; ++era) {
        const uint64_t ntp_seconds = ((uint64_t)era << 32) + seconds;
        if (ntp_seconds < epoch_offset) continue;
        const uint64_t candidate = ntp_seconds - epoch_offset;
        if (candidate >= earliest && candidate < latest) {
            *unix_seconds = candidate;
            return NTP_PACKET_OK;
        }
    }
    return NTP_PACKET_TIME_OUT_OF_RANGE;
}

const char *ntp_packet_error(NtpPacketResult result)
{
    switch (result) {
        case NTP_PACKET_OK: return "";
        case NTP_PACKET_BAD_ARGUMENT: return "Invalid NTP parser argument.";
        case NTP_PACKET_TOO_SHORT: return "NTP response is shorter than 48 bytes.";
        case NTP_PACKET_UNSYNCHRONIZED: return "NTP server is not synchronized.";
        case NTP_PACKET_BAD_VERSION: return "NTP response version must be 3 or 4.";
        case NTP_PACKET_BAD_MODE: return "NTP response is not in server mode.";
        case NTP_PACKET_BAD_STRATUM: return "NTP server stratum must be 1 through 15.";
        case NTP_PACKET_WRONG_ORIGIN: return "NTP response does not match the request.";
        case NTP_PACKET_ZERO_TIMESTAMP: return "NTP server returned an unknown timestamp.";
        case NTP_PACKET_TIME_OUT_OF_RANGE: return "NTP time is outside 2020 through 2099.";
        default: return "Unknown NTP parser error.";
    }
}
