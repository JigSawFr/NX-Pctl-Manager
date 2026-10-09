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

RescueMode rescue_request_mode(const char *content, size_t len)
{
    static const char word[] = "delete";
    const size_t n = sizeof(word) - 1;
    if (!content) return RescueMode_Unlock;
    for (size_t i = 0; i + n <= len; i++) {
        size_t k = 0;
        while (k < n && lower(content[i + k]) == word[k]) k++;
        if (k == n) return RescueMode_Delete;
    }
    return RescueMode_Unlock;
}

const char *rescue_mode_name(RescueMode mode)
{
    return mode == RescueMode_Delete ? "delete" : "unlock";
}

const char *rescue_result_name(RescueResult result)
{
    switch (result) {
    case RescueResult_Ok:    return "ok";
    case RescueResult_NoPin: return "no_pin";
    default:                 return "failed";
    }
}

size_t rescue_report_format(const RescueReport *r, char *buf, size_t size)
{
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    if (!r) return 0;
    int n = snprintf(buf, size, "mode=%s\nresult=%s\nrc=0x%08X\nunlocks=%u\n",
                     rescue_mode_name(r->mode), rescue_result_name(r->result),
                     (unsigned)r->rc, (unsigned)r->unlocks);
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
    RescueReport r = { RescueMode_Unlock, RescueResult_Failed, 0, 0 };
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
                else return false;
                has_result = true;
            } else if (equals(line, kn, "rc")) {
                if (!parse_u32(v, vn, &r.rc)) return false;
            } else if (equals(line, kn, "unlocks")) {
                if (!parse_u32(v, vn, &r.unlocks)) return false;
            }
        }
        i = end + 1;
    }
    if (!has_mode || !has_result) return false;
    *out = r;
    return true;
}
