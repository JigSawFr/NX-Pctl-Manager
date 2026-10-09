// rescue — request and report files (see rescue.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "rescue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

RescueMode rescue_request_mode(const char *content, size_t len)
{
    static const char word[] = "delete";
    const size_t n = sizeof(word) - 1;
    if (!content) return RescueMode_Unlock;
    size_t i = 0;
    // Notepad's byte-order mark, at the very start only.
    if (len >= 3 && (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF)
        i = 3;
    while (i < len) {
        size_t end = i;
        while (end < len && content[end] != '\n') end++;
        size_t b = i, e = end;
        while (b < e && is_space(content[b])) b++;
        while (e > b && is_space(content[e - 1])) e--;
        if (e - b == n) {
            size_t k = 0;
            while (k < n && lower(content[b + k]) == word[k]) k++;
            if (k == n) return RescueMode_Delete;
        }
        i = end + 1;
    }
    return RescueMode_Unlock;
}

const char *rescue_mode_name(RescueMode mode)
{
    return mode == RescueMode_Delete ? "delete" : "unlock";
}

const char *rescue_request_name(RescueRequest request)
{
    switch (request) {
    case RescueRequest_Removed: return "removed";
    case RescueRequest_Renamed: return "renamed";
    default:                    return "kept";
    }
}

const char *rescue_result_name(RescueResult result)
{
    switch (result) {
    case RescueResult_Ok:      return "ok";
    case RescueResult_NoPin:   return "no_pin";
    case RescueResult_Refused: return "refused";
    default:                   return "failed";
    }
}

size_t rescue_report_format(const RescueReport *r, char *buf, size_t size)
{
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    if (!r) return 0;
    int n = snprintf(buf, size, "mode=%s\nresult=%s\nrc=0x%08X\nunlocks=%u\nrequest=%s\n",
                     rescue_mode_name(r->mode), rescue_result_name(r->result),
                     (unsigned)r->rc, (unsigned)r->unlocks, rescue_request_name(r->request));
    if (n < 0 || (size_t)n >= size) {
        buf[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

// `value` (not NUL-terminated, `n` bytes) as an unsigned number: decimal, or
// hexadecimal after "0x". False for anything else or above 32 bits.
static bool parse_u32(const char *value, size_t n, uint32_t *out)
{
    char tmp[16];
    if (n == 0 || n >= sizeof(tmp)) return false;
    memcpy(tmp, value, n);
    tmp[n] = '\0';
    int base = 10;
    const char *digits = tmp;
    if (n > 2 && tmp[0] == '0' && (tmp[1] == 'x' || tmp[1] == 'X')) {
        base = 16;
        digits = tmp + 2;
    }
    for (const char *c = digits; *c; c++) {
        const bool dec = *c >= '0' && *c <= '9';
        const bool hex = (*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F');
        if (!(dec || (base == 16 && hex))) return false;
    }
    char *end = NULL;
    unsigned long long v = strtoull(digits, &end, base);
    if (!end || *end || v > 0xFFFFFFFFull) return false;
    *out = (uint32_t)v;
    return true;
}

static bool equals(const char *s, size_t n, const char *word)
{
    return strlen(word) == n && memcmp(s, word, n) == 0;
}

bool rescue_report_parse(const char *text, size_t len, RescueReport *out)
{
    if (!text || !out) return false;
    RescueReport r = { RescueMode_Unlock, RescueResult_Failed, 0, 0, RescueRequest_Removed };
    bool has_mode = false, has_result = false;
    size_t i = 0;
    while (i < len) {
        size_t end = i;
        while (end < len && text[end] != '\n') end++;
        size_t line_end = end;
        if (line_end > i && text[line_end - 1] == '\r') line_end--;
        const char *line = text + i;
        const size_t n = line_end - i;
        const char *eq = memchr(line, '=', n);
        if (eq) {
            const size_t kn = (size_t)(eq - line);
            const char *v = eq + 1;
            const size_t vn = n - kn - 1;
            if (equals(line, kn, "mode")) {
                if (equals(v, vn, "unlock"))      r.mode = RescueMode_Unlock;
                else if (equals(v, vn, "delete")) r.mode = RescueMode_Delete;
                else return false;
                has_mode = true;
            } else if (equals(line, kn, "result")) {
                if (equals(v, vn, "ok"))          r.result = RescueResult_Ok;
                else if (equals(v, vn, "no_pin")) r.result = RescueResult_NoPin;
                else if (equals(v, vn, "failed")) r.result = RescueResult_Failed;
                else if (equals(v, vn, "refused")) r.result = RescueResult_Refused;
                else return false;
                has_result = true;
            } else if (equals(line, kn, "rc")) {
                if (!parse_u32(v, vn, &r.rc)) return false;
            } else if (equals(line, kn, "unlocks")) {
                if (!parse_u32(v, vn, &r.unlocks)) return false;
            } else if (equals(line, kn, "request")) {
                if (equals(v, vn, "removed"))      r.request = RescueRequest_Removed;
                else if (equals(v, vn, "renamed")) r.request = RescueRequest_Renamed;
                else if (equals(v, vn, "kept"))    r.request = RescueRequest_Kept;
                else return false;
            }
        }
        i = end + 1;
    }
    if (!has_mode || !has_result) return false;
    *out = r;
    return true;
}
