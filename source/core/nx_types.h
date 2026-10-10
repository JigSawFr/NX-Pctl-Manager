// nx_types — common integer / Result types for the C service layer.
//
// On the Switch (and in the host unit tests, which ship a minimal fake
// <switch.h>) the real libnx header is used. On a desktop build (borealis
// PLATFORM_DESKTOP, used to exercise the UI without a console) we only need the
// handful of typedefs the UI-facing headers mention; the simulated backend in
// source/sim/ provides the implementations.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#if defined(__SWITCH__) || defined(NX_HOST_TEST)
#include <switch.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef u32      Result;
#ifndef R_SUCCEEDED
#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc)    ((rc) != 0)
#endif
#ifndef MAKEHOSVERSION
#define MAKEHOSVERSION(_major, _minor, _micro) (((u32)(_major) << 16) | ((u32)(_minor) << 8) | (u32)(_micro))
#define HOSVER_MAJOR(_v) (((_v) >> 16) & 0xFF)
#define HOSVER_MINOR(_v) (((_v) >> 8) & 0xFF)
#define HOSVER_MICRO(_v) ((_v) & 0xFF)
#endif
#endif

// Application-defined Result codes. Module 400 is outside every Horizon and
// libnx module, so these never collide with a real service error. Layout is the
// usual Horizon one: module in bits 0..8, description in bits 9..21.
#define NXM_MODULE 400u
#define NXM_RESULT(desc) ((Result)((NXM_MODULE & 0x1FFu) | (((u32)(desc) & 0x1FFFu) << 9)))
#define NXM_RC_READ_ONLY            NXM_RESULT(1)   // read-only mode (write_guard.h): no mutation allowed
#define NXM_RC_WRITE_GATED          NXM_RESULT(2)   // play timer active and not temporarily unlocked
#define NXM_RC_FW_UNSUPPORTED       NXM_RESULT(3)   // command / struct layout not known on this firmware
#define NXM_RC_UNLOCK_NOT_EFFECTIVE NXM_RESULT(4)   // 1201 returned OK but 1006 still reads false
#define NXM_RC_NOT_CUSTOM           NXM_RESULT(5)   // custom settings written while level != Custom
#define NXM_RC_INVALID_ARGUMENT     NXM_RESULT(6)
#define NXM_RC_AUTOCORRECT_OFF      NXM_RESULT(7)   // "Synchronise clock via Internet" is disabled
#define NXM_RC_STATE_UNKNOWN        NXM_RESULT(8)   // a gating read failed, refusing to write
#define NXM_RC_NOT_CONFIRMED        NXM_RESULT(9)   // the PIN asked before a change was not entered (write_guard.h)
#define NXM_RC_NO_PIN               NXM_RESULT(10)  // no parental-control PIN is set
#define NXM_RC_RELOCK_FAILED        NXM_RESULT(11)  // unlocked, then neither verified nor locked again: may still be unlocked
#define NXM_RC_NOT_APPLIED          NXM_RESULT(12)  // written, the console did not report it: the previous settings were put back
#define NXM_RC_NOT_SAVED            NXM_RESULT(13)  // what PlayGuard must put back later could not be saved to the SD card: nothing written
#define NXM_IS_APP_RESULT(rc)       (((rc) & 0x1FFu) == NXM_MODULE)
#define NXM_RESULT_DESC(rc)         (((rc) >> 9) & 0x1FFFu)
