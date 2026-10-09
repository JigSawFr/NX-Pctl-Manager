// rescue — the "last chance" shared by the rescue sysmodule (sysmodule/) and
// the app: the request file a parent puts on the SD card, and the report the
// sysmodule leaves for PlayGuard.
//
// A parent who forgot the PIN, while the play timer keeps every program from
// starting (PlayGuard included), puts a file named RESCUE (or RESCUE.txt) in
// sd:/switch/playguard/ from a computer and turns the console on:
//   - empty, or any text: the sysmodule unlocks parental controls temporarily
//     with the stored PIN (as the PIN screen would), so PlayGuard can open;
//   - the word "delete" in it: the sysmodule deletes every parental control.
// The request is removed before anything is done (it is used once), and the
// result is left in rescue_report.txt for PlayGuard, which shows it, records
// it in the change history and removes the report.
//
// Anyone who can edit the SD card can do this, as they can turn off
// PlayGuard's own PIN prompt (pin_lock.hpp): it keeps a child out, not
// someone with a computer. No PIN is ever written to either file.
//
// Plain C, no libnx: host-tested (tests/rescue).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// File names inside the data directory (sd:/switch/playguard/). The SD card's
// file system ignores case, so "rescue.txt" is found too.
#define RESCUE_REQUEST_NAME     "RESCUE"
#define RESCUE_REQUEST_NAME_TXT "RESCUE.txt"
#define RESCUE_REPORT_NAME      "rescue_report.txt"

typedef enum {
    RescueMode_Unlock = 0,   // unlock temporarily with the stored PIN
    RescueMode_Delete = 1,   // delete every parental control (1043)
} RescueMode;

typedef enum {
    RescueResult_Ok     = 0,
    RescueResult_NoPin  = 1,   // nothing to unlock: no PIN is set
    RescueResult_Failed = 2,   // `rc` says why
} RescueResult;

typedef struct {
    RescueMode   mode;
    RescueResult result;
    uint32_t     rc;        // the last failure, 0 when none
    uint32_t     unlocks;   // unlocks done (again when the system locked in between)
} RescueReport;

// The mode a request asks for: Delete when its first `len` bytes contain
// "delete" in any case, else Unlock (an empty file included).
RescueMode rescue_request_mode(const char *content, size_t len);

// "mode=unlock\nresult=ok\nrc=0x00000000\nunlocks=1\n" into buf. Returns the
// length written, or 0 when it does not fit (buf then holds "").
size_t rescue_report_format(const RescueReport *r, char *buf, size_t size);

// Back from rescue_report_format. Lines in any order, unknown keys and
// "\r" ignored; mode and result are required. False, and *out unchanged,
// for anything else.
bool rescue_report_parse(const char *text, size_t len, RescueReport *out);

const char *rescue_mode_name(RescueMode mode);         // "unlock" / "delete"
const char *rescue_result_name(RescueResult result);   // "ok" / "no_pin" / "failed"

#ifdef __cplusplus
}
#endif
