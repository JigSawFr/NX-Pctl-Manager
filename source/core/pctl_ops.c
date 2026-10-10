// PlayGuard — pctl service layer (see pctl_ops.h).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#include "pctl_ops.h"
#include "pure.h"
#include "write_guard.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * Command IDs for IParentalControlService.
 * Reference: https://switchbrew.org/wiki/Parental_Control_services
 * What was found on hardware is gathered in docs/parental-controls.md.
 * Checked against the command table up to 23.0.1 (PCTL_FW_TABLE_CHECKED: no ID
 * used here changed between 21.0.0 and 23.0.1; 23.0.0 only *added*
 * 1023/1412/1460/2025-2027/9407). Verified on a console up to 22.5.0
 * (PCTL_FW_TESTED_MAX): newer goes through the firmware gate.
 *
 *   1006 IsRestrictionTemporaryUnlocked   -> bool
 *   1007 RevertRestrictionTemporaryUnlocked (no args)
 *   1031 IsRestrictionEnabled             -> bool
 *   1032 GetSafetyLevel                   -> u32       1033 SetSafetyLevel <- u32
 *   1035 GetCurrentSettings               -> RestrictionSettings (3 bytes)
 *   1036 SetCustomSafetyLevelSettings     <- RestrictionSettings (3 bytes)
 *   1034 GetSafetyLevelSettings          <- u32 level -> RestrictionSettings (3 bytes)
 *   1037 GetDefaultRatingOrganization     -> u32       1038 SetDefaultRatingOrganization <- u32
 *   1039 GetFreeCommunicationApplicationListCount -> u32
 *   1044 GetFreeCommunicationApplicationList <- u32 offset -> u32 count + Out|MapAlias buffer
 *        (FreeCommunicationApplicationInfo, layout not documented: only dumped raw)
 *   1043 DeleteSettings                   (no args) privileged -- IRREVERSIBLE
 *   1903 GetExemptApplicationListCountForDebug [5.0.0+] -> u32, and
 *   1904 GetExemptApplicationListForDebug [5.0.0+], read like 1039 / 1044
 *        (no published signature: taken to mirror them; dumped raw, 1904
 *        only when 1903 counts an entry)
 *   Not called: 9401/9402 GetEvents(WithJson) [20.0.0+] and 1404/1411/1421
 *   (paired account): no published signature, and 1404 may go online.
 *   1062 GetStereoVisionRestriction       -> bool [4.0.0+]   1063 Set… <- bool
 *   1201 UnlockRestrictionTemporarily     <- PIN, In|HipcPointer buffer, NUL-terminated
 *                                            (MapAlias or no buffer => 0xF601 session closed;
 *                                             digits without '\0' => 0xF80E). Verified fw 22.1.0.
 *   1206 GetPinCodeLength                 -> u32 (0 == no PIN)
 *   1208 GetPinCode [4.0.0+]              -> u32 len + Out|HipcPointer buffer
 *   1403 IsPairingActive                  -> bool
 *   1406 GetSettingsLastUpdated           -> PosixTime
 *   1451 StartPlayTimer / 1452 StopPlayTimer (no args)
 *   1453 IsPlayTimerEnabled -> bool  1454 GetPlayTimerRemainingTime -> TimeSpan(ns)
 *   1455 IsRestrictedByPlayTimer -> bool   1458 IsPlayTimerAlarmDisabled -> bool [4.0.0+]
 *   1459 GetPlayTimerRemainingTimeDisplayInfo [20.0.0+] (0x20 bytes: u8 at 0, 2 with a limit
 *        today; the remaining ns at 0x10, as 1454 — docs/parental-controls.md)
 *   1460 GetWatcherStatusDisplayInfo [23.0.0+] (in 1 byte, out 0x18 bytes, layout unknown)
 *   1941 DeletePairing (no args) privileged
 *   1952 GetPlayTimerSpentTimeForTest -> TimeSpan   1953 SetPlayTimerAlarmDisabledForDebug <- bool
 *   1954 IsBedtimeAlarmEnabled -> bool [18.0.0+]  1956/1957 GetBedtimeAlarmTimeHour/Minute [18.0.0+]
 *   1958/1959 GetBedtimeAlarmResetTimeHour/Minute [20.0.0+]  1960 GetExtraPlayingTimeForDebug [20.0.0+]
 *   145601 GetPlayTimerSettings -> 0x44 bytes [21.0.0+ size]   195101 SetPlayTimerSettingsForDebug <- 0x44
 *   Never call 1456 / 1951: they are the [18.0.0-20.5.0] "Old" variants and close the session on 21+.
 *   1601/1602/1603 return 0x00010A8E on a normal console — not usable.
 *
 * Registering / changing the PIN goes through the OS library applet
 * (pctlauthRegisterPasscode); the direct SetPinCode command is rejected.
 *
 * The privileged calls only succeed under Atmosphère, where libnx's
 * pctlInitialize lands on a `pctl:a` / `pctl:s` session.
 */

// ---------------------------------------------------------------- session

static bool s_owned_open = false;

Result pctl_ops_init(void)
{
    if (s_owned_open) return 0;
    Result rc = pctlInitialize();
    if (R_SUCCEEDED(rc)) s_owned_open = true;
    return rc;
}

void pctl_ops_exit(void)
{
    if (!s_owned_open) return;
    pctlExit();
    s_owned_open = false;
}

Result pctl_ops_reinit(void)
{
    pctl_ops_exit();
    return pctl_ops_init();
}

// Small typed readers. Reading a bool through a u8 keeps the transfer size at
// one byte whatever the platform's sizeof(bool).
static Result rd_bool(Service *s, u32 cmd, bool *out)
{
    u8 v = 0;
    Result rc = serviceDispatchOut(s, cmd, v);
    if (R_SUCCEEDED(rc)) *out = v != 0;
    return rc;
}
static Result rd_u8(Service *s, u32 cmd, u8 *out)
{
    u8 v = 0;
    Result rc = serviceDispatchOut(s, cmd, v);
    if (R_SUCCEEDED(rc)) *out = v;
    return rc;
}
static Result rd_u32(Service *s, u32 cmd, u32 *out)
{
    u32 v = 0;
    Result rc = serviceDispatchOut(s, cmd, v);
    if (R_SUCCEEDED(rc)) *out = v;
    return rc;
}
static Result rd_u64(Service *s, u32 cmd, u64 *out)
{
    u64 v = 0;
    Result rc = serviceDispatchOut(s, cmd, v);
    if (R_SUCCEEDED(rc)) *out = v;
    return rc;
}

static void secure_zero(void *p, size_t n)
{
    volatile u8 *b = (volatile u8 *)p;
    while (n--) *b++ = 0;
}

// Runs one no-argument command in its own session. `relock`: locking again,
// which only read-only mode refuses (never the PIN check, write_guard.h).
static Result run_simple(u32 cmd, bool relock)
{
    Result rc = relock ? (core_read_only() ? NXM_RC_READ_ONLY : 0) : core_change_allowed();
    if (R_FAILED(rc)) return rc;
    rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    rc = serviceDispatch(pctlGetServiceSession_Service(), cmd);
    pctl_ops_exit();
    return rc;
}

// ---------------------------------------------------------------- status

// The fields every screen needs; status_read_rest adds the others.
static void status_read_core(Service *srv, PctlStatus *out)
{
    out->safety_level_ok        = R_SUCCEEDED(rd_u32 (srv, 1032, &out->safety_level));
    out->pin_length_ok          = R_SUCCEEDED(rd_u32 (srv, 1206, &out->pin_length));
    out->restriction_enabled_ok = R_SUCCEEDED(rd_bool(srv, 1031, &out->restriction_enabled));
    out->temp_unlocked_ok       = R_SUCCEEDED(rd_bool(srv, 1006, &out->temp_unlocked));
    out->pairing_active_ok      = R_SUCCEEDED(rd_bool(srv, 1403, &out->pairing_active));
}

static void status_read_rest(Service *srv, PctlStatus *out)
{
    out->rating_org_ok          = R_SUCCEEDED(rd_u32 (srv, 1037, &out->rating_org));
    out->free_comm_count_ok     = R_SUCCEEDED(rd_u32 (srv, 1039, &out->free_comm_count));
    out->last_updated_ok        = R_SUCCEEDED(rd_u64 (srv, 1406, &out->last_updated));

    u8 raw[3] = {0};
    if (R_SUCCEEDED(serviceDispatchOut(srv, 1035, raw))) {
        out->settings.rating_age                     = raw[0];
        out->settings.sns_post_restriction           = raw[1] != 0;
        out->settings.free_communication_restriction = raw[2] != 0;
        out->settings_ok = true;
    }
    if (hosversionAtLeast(4, 0, 0))
        out->stereo_vision_ok = R_SUCCEEDED(rd_bool(srv, 1062, &out->stereo_vision_restricted));
}

void pctl_status_fetch(PctlStatus *out)
{
    memset(out, 0, sizeof(*out));
    out->session_rc = pctl_ops_init();
    if (R_FAILED(out->session_rc)) return;
    Service *srv = pctlGetServiceSession_Service();
    status_read_core(srv, out);
    status_read_rest(srv, out);
    pctl_ops_exit();
}

Result pctl_lock_state(u32 *pin_length, bool *unlocked)
{
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();
    if (pin_length) rc = rd_u32(srv, 1206, pin_length);
    if (R_SUCCEEDED(rc) && unlocked) rc = rd_bool(srv, 1006, unlocked);
    pctl_ops_exit();
    return rc;
}

// ---------------------------------------------------------------- PIN / unlock

#define PIN_BUF 32

// GetPinCode (1208) into `pin` (PIN_BUF bytes, always NUL-terminated). The
// buffer is a HIPC *pointer* buffer, like 1201's (see below). The caller
// wipes `pin` and `*len`.
static Result read_pin(Service *srv, char *pin, u32 *len)
{
    memset(pin, 0, PIN_BUF);
    *len = 0;
    Result rc = serviceDispatchOut(srv, 1208, *len,
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_Out },
        .buffers      = { { pin, PIN_BUF } });
    pin[PIN_BUF - 1] = '\0';
    return rc;
}

Result pctl_set_pin(void)
{
    // The pctlauth applet opens its own privileged session: ours must be closed.
    pctl_ops_exit();
    Result rc = core_change_allowed();
    if (R_FAILED(rc)) return rc;
    return pctlauthRegisterPasscode();
}

Result pctl_ask_pin(void)
{
    // GetPinCodeLength first: the applet must not be shown without a PIN.
    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;
    u32 len = 0;
    rc = rd_u32(pctlGetServiceSession_Service(), 1206, &len);
    // The pctlauth applet opens its own privileged session: ours must be closed.
    pctl_ops_exit();
    if (R_FAILED(rc)) return rc;
    if (len == 0) return NXM_RC_NO_PIN;
    return pctlauthShowForConfiguration();
}

Result pctl_unlock_restriction_temporarily(void)
{
    pctl_ops_exit();   // nothing held while the PIN check may show its applet
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    // Two things had to be right (both learned the hard way on fw 22.1.0):
    //  - the buffers are HIPC *pointer* buffers (SfBufferAttr_HipcPointer), not
    //    map-alias — map-alias makes the sysmodule drop the session (0xF601);
    //  - the PIN is passed NUL-terminated (GetPinCodeLength + 1 bytes) — the
    //    bare digits return 0xF80E.
    // The PIN is read with GetPinCode (1208), handed straight back and wiped;
    // it never leaves this function (pctl_get_pin is the only other reader).
    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();

    char pin[PIN_BUF];
    u32 pin_len = 0;
    rc = read_pin(srv, pin, &pin_len);
    if (R_SUCCEEDED(rc)) {
        size_t n = (pin_len > 0 && pin_len < (u32)sizeof(pin)) ? ((size_t)pin_len + 1) : sizeof(pin);
        (void)n;   // only referenced inside the buffer descriptor below
        rc = serviceDispatch(srv, 1201,
            .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_In },
            .buffers      = { { pin, n } });
    }
    secure_zero(pin, sizeof(pin));
    secure_zero(&pin_len, sizeof(pin_len));

    if (R_SUCCEEDED(rc)) {
        bool unlocked = false;
        Result vr = rd_bool(srv, 1006, &unlocked);
        if (R_FAILED(vr)) {
            // 1201 went through but the state cannot be read back: the caller
            // treats this as a failure and will not lock again, so do it here
            // rather than leave the console unlocked without anyone knowing.
            // Should that fail too, say so: the console may still be unlocked.
            rc = R_SUCCEEDED(serviceDispatch(srv, 1007)) ? vr : NXM_RC_RELOCK_FAILED;
        } else if (!unlocked) {
            rc = NXM_RC_UNLOCK_NOT_EFFECTIVE;
        }
    }
    pctl_ops_exit();
    return rc;
}

Result pctl_get_pin(char *out, size_t out_size)
{
    if (out && out_size) secure_zero(out, out_size);
    pctl_ops_exit();   // nothing held while the PIN check may show its applet
    Result gate = core_change_allowed();   // showing the PIN counts as a change
    if (R_FAILED(gate)) return gate;
    if (!out || out_size == 0) return NXM_RC_INVALID_ARGUMENT;
    // Same session handling as the unlock above, where 1208 was validated.
    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;

    char pin[PIN_BUF];
    u32 pin_len = 0;
    rc = read_pin(pctlGetServiceSession_Service(), pin, &pin_len);
    if (R_SUCCEEDED(rc)) {
        size_t n = 0;
        while (n < PIN_BUF && pin[n]) n++;
        if (pin_len > 0 && pin_len < n) n = pin_len;
        // A Switch PIN is 4 to 8 digits; anything else is not shown.
        bool digits = n >= 4 && n <= 8;
        for (size_t i = 0; i < n && digits; i++) digits = pin[i] >= '0' && pin[i] <= '9';
        if (!digits)            rc = NXM_RC_STATE_UNKNOWN;
        else if (n >= out_size) rc = NXM_RC_INVALID_ARGUMENT;
        else {
            memcpy(out, pin, n);
            out[n] = '\0';
        }
    }
    secure_zero(pin, sizeof(pin));
    secure_zero(&pin_len, sizeof(pin_len));
    pctl_ops_exit();
    return rc;
}

Result pctl_relock(void)                     { return run_simple(1007, true); }
Result pctl_delete_parental_controls(void)   { return run_simple(1043, false); }
Result pctl_delete_pairing(void)             { return run_simple(1941, false); }

// ---------------------------------------------------------------- restrictions

Result pctl_get_level_settings(u32 level, PctlCustomSettings *out)
{
    if (!out || level > PctlSafetyLevel_Teen) return NXM_RC_INVALID_ARGUMENT;
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    u8 raw[3] = {0};
    rc = serviceDispatchInOut(pctlGetServiceSession_Service(), 1034, level, raw);
    pctl_ops_exit();
    if (R_SUCCEEDED(rc)) {
        out->rating_age                     = raw[0];
        out->sns_post_restriction           = raw[1] != 0;
        out->free_communication_restriction = raw[2] != 0;
    }
    return rc;
}

#define RATING_ORG_COUNT 13   // nn::ns::RatingOrganization, as pctl_rating_org_name names them

Result pctl_set_rating_org(u32 org)
{
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (org >= RATING_ORG_COUNT) return NXM_RC_INVALID_ARGUMENT;
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    rc = serviceDispatchIn(pctlGetServiceSession_Service(), 1038, org);
    pctl_ops_exit();
    return rc;
}

Result pctl_set_safety_level(u32 level)
{
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (level > PctlSafetyLevel_Teen) return NXM_RC_INVALID_ARGUMENT;
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    rc = serviceDispatchIn(pctlGetServiceSession_Service(), 1033, level);
    pctl_ops_exit();
    return rc;
}

Result pctl_set_custom_settings(const PctlCustomSettings *s)
{
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (!s || s->rating_age > 21) return NXM_RC_INVALID_ARGUMENT;
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();
    u32 level = 0;
    rc = rd_u32(srv, 1032, &level);
    if (R_SUCCEEDED(rc) && level != PctlSafetyLevel_Custom) rc = NXM_RC_NOT_CUSTOM;
    if (R_SUCCEEDED(rc)) {
        u8 raw[3] = { s->rating_age, (u8)(s->sns_post_restriction ? 1 : 0),
                      (u8)(s->free_communication_restriction ? 1 : 0) };
        rc = serviceDispatchIn(srv, 1036, raw);
    }
    pctl_ops_exit();
    return rc;
}

Result pctl_set_stereo_vision_restricted(bool restricted)
{
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (!hosversionAtLeast(4, 0, 0)) return NXM_RC_FW_UNSUPPORTED;
    Result rc = pctl_ops_init();
    if (R_FAILED(rc)) return rc;
    u8 v = restricted ? 1 : 0;
    rc = serviceDispatchIn(pctlGetServiceSession_Service(), 1063, v);
    pctl_ops_exit();
    return rc;
}

// ---------------------------------------------------------------- play timer

// PlayTimerSettings: layout and codec in pure.h. 1454 is in nanoseconds.

static bool pt_fw_supported(void) { return hosversionAtLeast(21, 0, 0); }

// Writing while the timer counts down destabilises Atmosphère. Every play-timer
// write checks in its own session, right before writing, so no caller can skip
// the gate: NXM_RC_WRITE_GATED while the timer is enabled (1453) or restricting
// (1455) and not temporarily unlocked (1006). `active` (may be NULL) gets
// whether the timer is enabled or restricting.
static Result pt_write_gate(Service *srv, bool *active)
{
    bool enabled = false, restricted = false, unlocked = false;
    Result rc = rd_bool(srv, 1453, &enabled);
    if (R_SUCCEEDED(rc)) rc = rd_bool(srv, 1455, &restricted);
    if (R_SUCCEEDED(rc)) rc = rd_bool(srv, 1006, &unlocked);
    if (R_FAILED(rc)) return rc;
    // Temporarily unlocked, 1453 reads false even with a limit set
    // (docs/parental-controls.md), and every write while the timer counts
    // goes through that unlock: count it as active too, or a failed read of
    // the block would let a zeroed one through.
    if (active) *active = enabled || restricted || unlocked;
    return ((enabled || restricted) && !unlocked) ? NXM_RC_WRITE_GATED : 0;
}

// A play-timer command (no argument, or the one byte `arg` points to) behind
// the change check and the gate above, in one session.
static Result pt_gated_command(u32 cmd, const u8 *arg)
{
    pctl_ops_exit();   // nothing held while the PIN check may show its applet
    Result rc = core_change_allowed();
    if (R_FAILED(rc)) return rc;
    rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();
    rc = pt_write_gate(srv, NULL);
    if (R_SUCCEEDED(rc)) rc = arg ? serviceDispatchIn(srv, cmd, *arg) : serviceDispatch(srv, cmd);
    pctl_ops_exit();
    return rc;
}

static void pt_init(PtState *out)
{
    memset(out, 0, sizeof(*out));
    for (int n = 0; n < 7; n++) out->day_min[n] = PT_DAY_NOLIMIT;
    out->fw_supported = pt_fw_supported();
}

// Everything but 1006 (the caller reads or copies it), 1458 and the bedtime
// reset time (1958/1959), which only the Play timer tab shows.
static void pt_read_core(Service *srv, PtState *out)
{
    out->enabled_rc = rd_bool(srv, 1453, &out->enabled);
    out->enabled_valid = R_SUCCEEDED(out->enabled_rc);
    out->restricted_rc = rd_bool(srv, 1455, &out->restricted);
    out->restricted_valid = R_SUCCEEDED(out->restricted_rc);
    out->remaining_rc = rd_u64(srv, 1454, &out->remaining_ns);
    out->remaining_valid = R_SUCCEEDED(out->remaining_rc);

    u16 c[PT_U16_COUNT];
    memset(c, 0, sizeof(c));
    out->config_rc = serviceDispatchOut(srv, 145601, c);
    if (R_SUCCEEDED(out->config_rc)) {
        memcpy(out->block, c, sizeof(out->block));
        // Not the known layout: nothing decoded from it is shown or reused
        // (the block itself is kept, as read).
        if (!pt_plausible(c)) out->config_rc = NXM_RC_PT_NOT_UNDERSTOOD;
    }
    if (R_SUCCEEDED(out->config_rc)) {
        out->valid = true;
        pt_decode(c, out->day_min);
        pt_bedtime_decode(c, out->bed);
    }

    bool be = false;
    u8 h = 0, m = 0;
    if (R_SUCCEEDED(rd_bool(srv, 1954, &be)) && R_SUCCEEDED(rd_u8(srv, 1956, &h)) &&
        R_SUCCEEDED(rd_u8(srv, 1957, &m)) && h < 24 && m < 60) {
        out->bedtime_valid = true;
        out->bedtime_enabled = be;
        out->bedtime_hour = h;
        out->bedtime_minute = m;
    }
}

static void pt_read_rest(Service *srv, PtState *out)
{
    out->alarm_disabled_valid = R_SUCCEEDED(rd_bool(srv, 1458, &out->alarm_disabled));
    u8 h = 0, m = 0;
    if (hosversionAtLeast(20, 0, 0) && R_SUCCEEDED(rd_u8(srv, 1958, &h)) &&
        R_SUCCEEDED(rd_u8(srv, 1959, &m)) && h < 24 && m < 60) {
        out->bedtime_reset_valid = true;
        out->bedtime_reset_hour = h;
        out->bedtime_reset_minute = m;
    }
}

void pctl_play_timer_query(PtState *out)
{
    pt_init(out);
    if (!out->fw_supported) return;

    out->session_rc = pctl_ops_reinit();
    if (R_FAILED(out->session_rc)) return;
    out->session_valid = true;
    Service *srv = pctlGetServiceSession_Service();
    out->temporary_unlocked_rc = rd_bool(srv, 1006, &out->temporary_unlocked);
    out->temporary_unlocked_valid = R_SUCCEEDED(out->temporary_unlocked_rc);
    pt_read_core(srv, out);
    pt_read_rest(srv, out);
    pctl_ops_exit();
}

void pctl_overview_fetch(PctlStatus *status, PtState *pt)
{
    memset(status, 0, sizeof(*status));
    pt_init(pt);
    status->session_rc = pctl_ops_reinit();
    if (R_FAILED(status->session_rc)) {
        pt->session_rc = status->session_rc;
        return;
    }
    Service *srv = pctlGetServiceSession_Service();
    status_read_core(srv, status);
    if (pt->fw_supported) {
        pt->session_rc = status->session_rc;
        pt->session_valid = true;
        // 1006 was just read for the status: the same answer.
        pt->temporary_unlocked_valid = status->temp_unlocked_ok;
        pt->temporary_unlocked = status->temp_unlocked;
        pt->temporary_unlocked_rc = status->temp_unlocked_ok ? 0 : NXM_RC_STATE_UNKNOWN;
        pt_read_core(srv, pt);
        // The Overview warns while the "time's up" alarm is off.
        pt->alarm_disabled_valid = R_SUCCEEDED(rd_bool(srv, 1458, &pt->alarm_disabled));
    }
    pctl_ops_exit();
}

Result pctl_play_timer_set_days(const u16 days_min[7])
{
    pctl_ops_exit();   // nothing held while the PIN check may show its applet
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (!pt_fw_supported()) return NXM_RC_FW_UNSUPPORTED;
    for (int n = 0; n < 7; n++)
        if (days_min[n] != PT_DAY_NOLIMIT && days_min[n] > 1440) return NXM_RC_INVALID_ARGUMENT;

    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();

    bool active = false;
    rc = pt_write_gate(srv, &active);
    if (R_FAILED(rc)) {
        pctl_ops_exit();
        return rc;
    }

    // Start from the settings as they are, so the fields this app does not
    // understand survive. Should the read fail while the timer is off, fall
    // back to the layout observed on hardware (what earlier versions always
    // wrote): there is nothing set to lose. While it is on, the block holds
    // what the companion app set, which a zeroed block would wipe: refuse,
    // unless no day keeps a limit (pt_encode then writes all zeros anyway).
    u16 c[PT_U16_COUNT];
    memset(c, 0, sizeof(c));
    const bool have_current = R_SUCCEEDED(serviceDispatchOut(srv, 145601, c));
    // Read, but not a layout this app knows: writing from it could break what
    // it holds, and a zeroed block would wipe it.
    if (have_current && !pt_plausible(c)) {
        pctl_ops_exit();
        return NXM_RC_PT_NOT_UNDERSTOOD;
    }
    if (!have_current) {
        memset(c, 0, sizeof(c));
        bool any = false;
        for (int n = 0; n < 7; n++) if (days_min[n] != PT_DAY_NOLIMIT) any = true;
        if (active && any) {
            pctl_ops_exit();
            return NXM_RC_STATE_UNKNOWN;
        }
    }
    u16 before[PT_U16_COUNT];
    memcpy(before, c, sizeof(before));
    pt_encode(c, days_min);
    rc = serviceDispatchIn(srv, 195101, c);
    if (R_FAILED(rc)) {
        pctl_ops_exit();
        return rc;
    }

    // 195101 is a debug command: read the block back, as the bedtime write
    // does, so "written" is never said of limits the console did not take.
    // An unreadable block proves nothing either way: the write stands.
    u16 back[PT_U16_COUNT], want[7], got[7];
    if (R_SUCCEEDED(serviceDispatchOut(srv, 145601, back))) {
        pt_decode(c, want);
        pt_decode(back, got);
        if (memcmp(want, got, sizeof(want)) != 0) {
            if (have_current) rc = serviceDispatchIn(srv, 195101, before);
            pctl_ops_exit();
            return R_FAILED(rc) ? rc : NXM_RC_NOT_APPLIED;
        }
    }
    pctl_ops_exit();
    return 0;
}

Result pctl_play_timer_set_uniform(u16 minutes)
{
    u16 d[7];
    for (int i = 0; i < 7; i++) d[i] = minutes;
    return pctl_play_timer_set_days(d);
}

Result pctl_play_timer_clear(void)
{
    u16 d[7];
    for (int i = 0; i < 7; i++) d[i] = PT_DAY_NOLIMIT;
    return pctl_play_timer_set_days(d);
}

// What 1954/1956/1957 report is `b` (the time only matters while it is on).
static bool bedtime_reported(const PtBedtime *b, bool on, u8 h, u8 m)
{
    return on == b->on && (!on || (h == b->hour && m == b->minute));
}

Result pctl_play_timer_set_bedtime(const PtBedtime bed[7], int today)
{
    pctl_ops_exit();   // nothing held while the PIN check may show its applet
    Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    if (!pt_fw_supported()) return NXM_RC_FW_UNSUPPORTED;
    if (today < 0 || today > 6) return NXM_RC_INVALID_ARGUMENT;
    for (int n = 0; n < 7; n++)
        if (!pt_bedtime_ok(&bed[n])) return NXM_RC_INVALID_ARGUMENT;

    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) return rc;
    Service *srv = pctlGetServiceSession_Service();
    rc = pt_write_gate(srv, NULL);
    if (R_FAILED(rc)) {
        pctl_ops_exit();
        return rc;
    }

    // Only ever from the block as it is: a zeroed one would wipe the limits.
    u16 before[PT_U16_COUNT], c[PT_U16_COUNT];
    memset(before, 0, sizeof(before));
    if (R_FAILED(serviceDispatchOut(srv, 145601, before))) {
        pctl_ops_exit();
        return NXM_RC_STATE_UNKNOWN;
    }
    if (!pt_plausible(before)) {
        pctl_ops_exit();
        return NXM_RC_PT_NOT_UNDERSTOOD;
    }
    memcpy(c, before, sizeof(c));
    pt_bedtime_encode(c, bed);
    if (memcmp(c, before, sizeof(c)) == 0) {
        pctl_ops_exit();
        return 0;
    }
    rc = serviceDispatchIn(srv, 195101, c);
    if (R_FAILED(rc)) {
        pctl_ops_exit();
        return rc;
    }

    bool on = false;
    u8 h = 0, m = 0;
    const bool read = R_SUCCEEDED(rd_bool(srv, 1954, &on)) && R_SUCCEEDED(rd_u8(srv, 1956, &h)) &&
                      R_SUCCEEDED(rd_u8(srv, 1957, &m));
    if (read && (bedtime_reported(&bed[today], on, h, m) || bedtime_reported(&bed[(today + 6) % 7], on, h, m))) {
        pctl_ops_exit();
        return 0;
    }
    rc = serviceDispatchIn(srv, 195101, before);
    pctl_ops_exit();
    return R_FAILED(rc) ? rc : NXM_RC_NOT_APPLIED;
}

Result pctl_play_timer_set_alarm_disabled(bool disabled)
{
    const u8 v = disabled ? 1 : 0;
    return pt_gated_command(1953, &v);
}

Result pctl_play_timer_start(void) { return pt_gated_command(1451, NULL); }
Result pctl_play_timer_stop(void)  { return pt_gated_command(1452, NULL); }

// Every read of a PtSample failed with `rc`.
static void pt_sample_failed(PtSample *out, Result rc)
{
    out->session_rc = out->unlocked_rc = out->enabled_rc = out->restricted_rc = out->alarm_off_rc =
        out->remaining_rc = out->block_rc = out->bedtime_rc = out->display_rc = out->spent_rc =
            out->extra_rc = rc;
}

void pctl_play_timer_sample(PtSample *out)
{
    memset(out, 0, sizeof(*out));
    // Below 21.0.0 no play-timer command at all, as the diagnostic report.
    if (!pt_fw_supported()) {
        pt_sample_failed(out, NXM_RC_FW_UNSUPPORTED);
        return;
    }
    Result rc = pctl_ops_reinit();
    if (R_FAILED(rc)) {
        pt_sample_failed(out, rc);
        return;
    }
    Service *srv = pctlGetServiceSession_Service();
    out->unlocked_rc   = rd_bool(srv, 1006, &out->unlocked);
    out->enabled_rc    = rd_bool(srv, 1453, &out->enabled);
    out->restricted_rc = rd_bool(srv, 1455, &out->restricted);
    out->alarm_off_rc  = rd_bool(srv, 1458, &out->alarm_off);
    out->remaining_rc  = rd_u64(srv, 1454, &out->remaining_ns);
    out->block_rc      = serviceDispatchOut(srv, 145601, out->block);
    out->bedtime_rc    = rd_bool(srv, 1954, &out->bedtime_on);
    if (R_SUCCEEDED(out->bedtime_rc)) out->bedtime_rc = rd_u8(srv, 1956, &out->bedtime_hour);
    if (R_SUCCEEDED(out->bedtime_rc)) out->bedtime_rc = rd_u8(srv, 1957, &out->bedtime_minute);

    // The test / debug getters in their own session, as the report reads them.
    out->session_rc = pctl_ops_reinit();
    if (R_FAILED(out->session_rc)) {
        out->display_rc = out->spent_rc = out->extra_rc = out->session_rc;
        return;
    }
    srv = pctlGetServiceSession_Service();
    out->display_rc = serviceDispatchOut(srv, 1459, out->display);
    out->spent_rc   = rd_u64(srv, 1952, &out->spent_ns);
    out->extra_rc   = rd_u64(srv, 1960, &out->extra_ns);
    pctl_ops_exit();
}

// ---------------------------------------------------------------- diagnostics

static void rep(char **p, char *end, const char *fmt, ...)
{
    if (*p >= end) return;
    size_t available = (size_t)(end - *p);
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(*p, available, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n >= available) *p = end;
        else *p += n;
    }
}

static void rep_hex(char **p, char *e, const u8 *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        rep(p, e, "%02X ", b[i]);
        if ((i & 15) == 15) rep(p, e, "\n");
    }
    rep(p, e, "\n");
}

static void rep_bool(char **p, char *e, Service *srv, u32 cmd, const char *name)
{
    bool b = false;
    Result r = rd_bool(srv, cmd, &b);
    rep(p, e, "%6u %-38s rc=0x%08X  %s\n", (unsigned)cmd, name, (unsigned)r,
        R_SUCCEEDED(r) ? (b ? "true" : "false") : "-");
}

static void rep_u32(char **p, char *e, Service *srv, u32 cmd, const char *name)
{
    u32 v = 0;
    Result r = rd_u32(srv, cmd, &v);
    rep(p, e, "%6u %-38s rc=0x%08X  ", (unsigned)cmd, name, (unsigned)r);
    if (R_SUCCEEDED(r)) rep(p, e, "%u\n", (unsigned)v); else rep(p, e, "-\n");
}

static void rep_u64(char **p, char *e, Service *srv, u32 cmd, const char *name)
{
    u64 v = 0;
    Result r = rd_u64(srv, cmd, &v);
    rep(p, e, "%6u %-38s rc=0x%08X  ", (unsigned)cmd, name, (unsigned)r);
    if (R_SUCCEEDED(r)) rep(p, e, "0x%016llX (%llu)\n", (unsigned long long)v, (unsigned long long)v);
    else rep(p, e, "-\n");
}

// Opens a fresh session for the next group of commands. Returns NULL (and
// records the failure) when it cannot; the caller then stops the dump.
static Service *dump_session(char **p, char *e, const char *group)
{
    Result ir = pctl_ops_reinit();
    if (R_FAILED(ir)) {
        rep(p, e, "reconnect before %s failed: 0x%08X\n", group, (unsigned)ir);
        return NULL;
    }
    return pctlGetServiceSession_Service();
}

void pctl_dump(char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0) { pctl_ops_exit(); return; }
    char *p = buf, *e = buf + bufsz;
    buf[0] = '\0';
    Service *srv;

    rep(&p, e, "=== pctl state (read-only) ===\n");

    if (!(srv = dump_session(&p, e, "status"))) goto done;
    rep_u32 (&p, e, srv, 1032, "GetSafetyLevel");
    rep_bool(&p, e, srv, 1031, "IsRestrictionEnabled");
    rep_bool(&p, e, srv, 1006, "IsRestrictionTemporaryUnlocked");
    // Whether a PIN is set, not its length: a report may be posted online,
    // and the length narrows the guessing.
    { u32 len = 0; Result r = rd_u32(srv, 1206, &len);
      rep(&p, e, "%6u %-38s rc=0x%08X  %s\n", 1206u, "GetPinCodeLength", (unsigned)r,
          R_SUCCEEDED(r) ? (len ? "set (length not recorded)" : "none") : "-"); }
    rep_u32 (&p, e, srv, 1037, "GetDefaultRatingOrganization");
    rep_u32 (&p, e, srv, 1039, "GetFreeCommunicationApplicationListCount");
    rep_bool(&p, e, srv, 1403, "IsPairingActive");
    rep_u64 (&p, e, srv, 1406, "GetSettingsLastUpdated");
    { u8 raw[3] = {0}; Result r = serviceDispatchOut(srv, 1035, raw);
      rep(&p, e, "%6u %-38s rc=0x%08X  ", 1035u, "GetCurrentSettings", (unsigned)r);
      if (R_SUCCEEDED(r)) rep(&p, e, "rating_age=%u sns=%u comm=%u\n", raw[0], raw[1], raw[2]);
      else rep(&p, e, "-\n"); }
    if (hosversionAtLeast(4, 0, 0)) rep_bool(&p, e, srv, 1062, "GetStereoVisionRestriction");
    for (u32 level = PctlSafetyLevel_YoungChild; level <= PctlSafetyLevel_Teen; level++) {
        u8 raw[3] = {0};
        Result r = serviceDispatchInOut(srv, 1034, level, raw);
        rep(&p, e, "%6u GetSafetyLevelSettings(%u)%-15s rc=0x%08X  ", 1034u, (unsigned)level, "", (unsigned)r);
        if (R_SUCCEEDED(r)) rep(&p, e, "rating_age=%u sns=%u comm=%u\n", raw[0], raw[1], raw[2]);
        else rep(&p, e, "-\n");
    }

    // Software allowed to communicate: the entry layout is not documented,
    // so the bytes as returned (an application ID per entry is expected).
    if (!(srv = dump_session(&p, e, "1044"))) goto done;
    { u8 list[0x200]; memset(list, 0, sizeof(list));
      u32 offset = 0, count = 0;
      Result r = serviceDispatchInOut(srv, 1044, offset, count,
          .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
          .buffers      = { { list, sizeof(list) } });
      rep(&p, e, "%6u %-38s rc=0x%08X  ", 1044u, "GetFreeCommunicationApplicationList(0)", (unsigned)r);
      if (R_SUCCEEDED(r)) {
          rep(&p, e, "count=%u\n", (unsigned)count);
          size_t n = (size_t)count * 0x20;
          if (n > sizeof(list)) n = sizeof(list);
          if (n) rep_hex(&p, e, list, n);
      } else {
          rep(&p, e, "-\n");
      } }

    // Software exempt from the restrictions (a debug list, expected empty).
    if (!(srv = dump_session(&p, e, "1903"))) goto done;
    { u32 exempt = 0;
      Result r = rd_u32(srv, 1903, &exempt);
      rep(&p, e, "%6u %-38s rc=0x%08X  ", 1903u, "GetExemptApplicationListCountForDebug", (unsigned)r);
      if (R_SUCCEEDED(r)) rep(&p, e, "%u\n", (unsigned)exempt); else rep(&p, e, "-\n");
      if (R_SUCCEEDED(r) && exempt > 0) {
          if (!(srv = dump_session(&p, e, "1904"))) goto done;
          u8 list[0x200]; memset(list, 0, sizeof(list));
          u32 offset = 0, count = 0;
          Result lr = serviceDispatchInOut(srv, 1904, offset, count,
              .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
              .buffers      = { { list, sizeof(list) } });
          rep(&p, e, "%6u %-38s rc=0x%08X  ", 1904u, "GetExemptApplicationListForDebug(0)", (unsigned)lr);
          if (R_SUCCEEDED(lr)) {
              rep(&p, e, "count=%u\n", (unsigned)count);
              size_t n = (size_t)count * 8;   // an application ID each, expected
              if (n > sizeof(list)) n = sizeof(list);
              if (n) rep_hex(&p, e, list, n);
          } else {
              rep(&p, e, "-\n");
          }
      } }

    // GetPinCode is probed for compatibility only: the output buffer and the
    // returned length are wiped and never recorded.
    if (!(srv = dump_session(&p, e, "1208"))) goto done;
    { char pin[PIN_BUF]; u32 len = 0;
      Result r = read_pin(srv, pin, &len);
      secure_zero(pin, sizeof(pin));
      secure_zero(&len, sizeof(len));
      rep(&p, e, "%6u %-38s rc=0x%08X  content=not recorded\n", 1208u, "GetPinCode", (unsigned)r); }

    rep(&p, e, "\n=== play timer ===\n");
    if (!pt_fw_supported()) {
        rep(&p, e, "firmware below 21.0.0: PlayTimerSettings layout unknown, skipped\n");
        goto done;
    }
    if (!(srv = dump_session(&p, e, "play timer"))) goto done;
    rep_bool(&p, e, srv, 1453, "IsPlayTimerEnabled");
    rep_bool(&p, e, srv, 1455, "IsRestrictedByPlayTimer");
    rep_bool(&p, e, srv, 1458, "IsPlayTimerAlarmDisabled");
    rep_u64 (&p, e, srv, 1454, "GetPlayTimerRemainingTime (ns)");
    rep_bool(&p, e, srv, 1954, "IsBedtimeAlarmEnabled");
    { u8 h = 0, m = 0; Result rh = rd_u8(srv, 1956, &h), rm = rd_u8(srv, 1957, &m);
      rep(&p, e, "  1956/1957 BedtimeAlarmTime hour/minute       rc=0x%08X/0x%08X  %02u:%02u\n",
          (unsigned)rh, (unsigned)rm, h, m); }
    if (hosversionAtLeast(20, 0, 0)) {
        u8 h = 0, m = 0; Result rh = rd_u8(srv, 1958, &h), rm = rd_u8(srv, 1959, &m);
        rep(&p, e, "  1958/1959 BedtimeAlarmResetTime hour/minute  rc=0x%08X/0x%08X  %02u:%02u\n",
            (unsigned)rh, (unsigned)rm, h, m);
    }

    if (!(srv = dump_session(&p, e, "145601"))) goto done;
    { u8 b[0x44]; memset(b, 0xCC, sizeof(b));
      Result r = serviceDispatchOut(srv, 145601, b);
      rep(&p, e, "\n145601 GetPlayTimerSettings rc=0x%08X (0x44 bytes)\n", (unsigned)r);
      if (R_SUCCEEDED(r)) {
          rep_hex(&p, e, b, sizeof(b));
          u16 c[PT_U16_COUNT], d[7];
          memcpy(c, b, sizeof(c));
          pt_decode(c, d);
          rep(&p, e, "decoded per-day minutes Sun..Sat:");
          for (int n = 0; n < 7; n++) {
              if (d[n] != PT_DAY_NOLIMIT) rep(&p, e, " %u", (unsigned)d[n]);
              else rep(&p, e, " -");
          }
          rep(&p, e, "   header=%04X %04X\n", c[0], c[1]);
          PtBedtime bt[7];
          pt_bedtime_decode(c, bt);
          rep(&p, e, "decoded bedtime Sun..Sat (alarm/allowed again):");
          for (int n = 0; n < 7; n++)
              rep(&p, e, " %s%02u:%02u/%02u:%02u", bt[n].on ? "" : "off ", bt[n].hour, bt[n].minute,
                  bt[n].end_hour, bt[n].end_minute);
          rep(&p, e, "\n");
      } }

    if (hosversionAtLeast(20, 0, 0)) {
        if (!(srv = dump_session(&p, e, "1459"))) goto done;
        u8 b[0x20]; memset(b, 0, sizeof(b));
        Result r = serviceDispatchOut(srv, 1459, b);
        rep(&p, e, "\n1459 GetPlayTimerRemainingTimeDisplayInfo rc=0x%08X (0x20 bytes)\n", (unsigned)r);
        if (R_SUCCEEDED(r)) rep_hex(&p, e, b, sizeof(b));
    }
    if (hosversionAtLeast(23, 0, 0)) {
        if (!(srv = dump_session(&p, e, "1460"))) goto done;
        u8 in = 0;
        u8 b[0x18]; memset(b, 0, sizeof(b));
        Result r = serviceDispatchInOut(srv, 1460, in, b);
        rep(&p, e, "\n1460 GetWatcherStatusDisplayInfo(0) rc=0x%08X (0x18 bytes)\n", (unsigned)r);
        if (R_SUCCEEDED(r)) rep_hex(&p, e, b, sizeof(b));
    }

    // Test / debug getters: raw values, interpretation unverified on hardware.
    if (!(srv = dump_session(&p, e, "1952"))) goto done;
    rep(&p, e, "\n");
    rep_u64(&p, e, srv, 1952, "GetPlayTimerSpentTimeForTest (raw)");
    if (hosversionAtLeast(20, 0, 0)) {
        if (!(srv = dump_session(&p, e, "1960"))) goto done;
        rep_u64(&p, e, srv, 1960, "GetExtraPlayingTimeForDebug (raw)");
    }

done:
    pctl_ops_exit();
    rep(&p, e, "\nTool-owned pctl session released.\n");
}
