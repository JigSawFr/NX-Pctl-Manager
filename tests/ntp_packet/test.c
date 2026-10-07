#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ntp_packet.h"

static const uint8_t cookie[8] = {0x17, 0x92, 0xab, 0x04, 0xff, 0x80, 0x01, 0x61};

static void write_be32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static void response(uint8_t *packet, uint64_t unix_seconds, uint32_t fraction)
{
    memset(packet, 0, NTP_PACKET_SIZE);
    packet[0] = 0x24; // NTPv4 server, synchronized.
    packet[1] = 2;
    memcpy(packet + 24, cookie, sizeof(cookie));
    write_be32(packet + 40, (uint32_t)(unix_seconds + UINT64_C(2208988800)));
    write_be32(packet + 44, fraction);
}

static void expect(const uint8_t *packet, size_t length, NtpPacketResult result)
{
    uint64_t seconds = UINT64_MAX;
    assert(ntp_packet_parse(packet, length, cookie, &seconds) == result);
    if (result != NTP_PACKET_OK) assert(seconds == 0);
}

static void test_request_and_sizes(void)
{
    uint8_t packet[64];
    ntp_packet_make_request(packet, cookie);
    assert(packet[0] == 0x23);
    for (unsigned i = 1; i < 40; ++i) assert(packet[i] == 0);
    assert(memcmp(packet + 40, cookie, sizeof(cookie)) == 0);
    response(packet, UINT64_C(1790553600), 0);
    for (size_t length = 0; length < 48; ++length)
        expect(packet, length, NTP_PACKET_TOO_SHORT);
    expect(packet, 48, NTP_PACKET_OK);
    memset(packet + 48, 0xa5, 16);
    expect(packet, sizeof(packet), NTP_PACKET_OK);

    uint64_t seconds = UINT64_MAX;
    assert(ntp_packet_parse(NULL, 48, cookie, &seconds) == NTP_PACKET_BAD_ARGUMENT);
    assert(seconds == 0);
    assert(ntp_packet_parse(packet, 48, NULL, &seconds) == NTP_PACKET_BAD_ARGUMENT);
    assert(ntp_packet_parse(packet, 48, cookie, NULL) == NTP_PACKET_BAD_ARGUMENT);
}

static void test_header_and_cookie(void)
{
    uint8_t packet[48];
    for (unsigned leap = 0; leap < 4; ++leap) {
        response(packet, UINT64_C(1790553600), 0);
        packet[0] |= (uint8_t)(leap << 6);
        expect(packet, sizeof(packet), leap == 3 ? NTP_PACKET_UNSYNCHRONIZED : NTP_PACKET_OK);
    }
    for (unsigned version = 0; version < 8; ++version) {
        response(packet, UINT64_C(1790553600), 0);
        packet[0] = (uint8_t)((version << 3) | 4);
        expect(packet, sizeof(packet), version == 3 || version == 4 ? NTP_PACKET_OK : NTP_PACKET_BAD_VERSION);
    }
    for (unsigned mode = 0; mode < 8; ++mode) {
        response(packet, UINT64_C(1790553600), 0);
        packet[0] = (uint8_t)((4 << 3) | mode);
        expect(packet, sizeof(packet), mode == 4 ? NTP_PACKET_OK : NTP_PACKET_BAD_MODE);
    }
    const unsigned strata[] = {0, 1, 2, 15, 16, 255};
    for (unsigned i = 0; i < sizeof(strata) / sizeof(strata[0]); ++i) {
        response(packet, UINT64_C(1790553600), 0);
        packet[1] = (uint8_t)strata[i];
        expect(packet, sizeof(packet), strata[i] >= 1 && strata[i] <= 15 ? NTP_PACKET_OK : NTP_PACKET_BAD_STRATUM);
    }
    for (unsigned i = 0; i < sizeof(cookie); ++i) {
        response(packet, UINT64_C(1790553600), 0);
        packet[24 + i] ^= 1;
        expect(packet, sizeof(packet), NTP_PACKET_WRONG_ORIGIN);
    }
    response(packet, UINT64_C(1790553600), 0);
    memset(packet + 40, 0, 8);
    expect(packet, sizeof(packet), NTP_PACKET_ZERO_TIMESTAMP);
}

static void test_dates_and_era(void)
{
    uint8_t packet[48];
    uint64_t seconds = 0;
    const uint64_t valid_dates[] = {
        UINT64_C(1577836800), // 2020-01-01 00:00:00 UTC.
        UINT64_C(1790553600), // 2026-09-28 00:00:00 UTC.
        UINT64_C(2085978495), // Last second of NTP era 0.
        UINT64_C(2085978496), // First second of era 1; fractional part nonzero.
        UINT64_C(2085978497),
        UINT64_C(4102444799)  // 2099-12-31 23:59:59 UTC.
    };
    for (unsigned i = 0; i < sizeof(valid_dates) / sizeof(valid_dates[0]); ++i) {
        response(packet, valid_dates[i], 1);
        assert(ntp_packet_parse(packet, sizeof(packet), cookie, &seconds) == NTP_PACKET_OK);
        assert(seconds == valid_dates[i]);
    }
    // Test actual wire values at rollover independently of the date encoder.
    response(packet, UINT64_C(1790553600), 0);
    memset(packet + 40, 0xff, 4);
    memset(packet + 44, 0, 4);
    assert(ntp_packet_parse(packet, sizeof(packet), cookie, &seconds) == NTP_PACKET_OK);
    assert(seconds == UINT64_C(2085978495));
    memset(packet + 40, 0, 8);
    packet[47] = 1;
    assert(ntp_packet_parse(packet, sizeof(packet), cookie, &seconds) == NTP_PACKET_OK);
    assert(seconds == UINT64_C(2085978496));

    const uint64_t invalid_dates[] = {UINT64_C(1577836799), UINT64_C(4102444800)};
    for (unsigned i = 0; i < sizeof(invalid_dates) / sizeof(invalid_dates[0]); ++i) {
        response(packet, invalid_dates[i], 1);
        expect(packet, sizeof(packet), NTP_PACKET_TIME_OUT_OF_RANGE);
    }
}

int main(void)
{
    test_request_and_sizes();
    test_header_and_cookie();
    test_dates_and_era();
    puts("NTP packet validation and era assertions passed");
    return 0;
}
