// Copyright (C) 2026 Taylor. GPLv3-or-later (see LICENSE).
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NTP_PACKET_SIZE 48u
#define NTP_COOKIE_SIZE 8u

typedef enum {
    NTP_PACKET_OK = 0,
    NTP_PACKET_BAD_ARGUMENT,
    NTP_PACKET_TOO_SHORT,
    NTP_PACKET_UNSYNCHRONIZED,
    NTP_PACKET_BAD_VERSION,
    NTP_PACKET_BAD_MODE,
    NTP_PACKET_BAD_STRATUM,
    NTP_PACKET_WRONG_ORIGIN,
    NTP_PACKET_ZERO_TIMESTAMP,
    NTP_PACKET_TIME_OUT_OF_RANGE
} NtpPacketResult;

void ntp_packet_make_request(uint8_t request[NTP_PACKET_SIZE],
                             const uint8_t cookie[NTP_COOKIE_SIZE]);
NtpPacketResult ntp_packet_parse(const uint8_t *packet, size_t length,
                                 const uint8_t cookie[NTP_COOKIE_SIZE],
                                 uint64_t *unix_seconds);
const char *ntp_packet_error(NtpPacketResult result);

#ifdef __cplusplus
}
#endif
