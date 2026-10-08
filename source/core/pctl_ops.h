// PlayGuard — wrappers around the system parental-control (`pctl`) service.
// All user-facing strings live in the UI layer; this layer is data only.
//
// Session ownership: `pctl:a` accepts a single session. Holding it while the
// system wants it (HOME-menu PIN prompt, pctlauth applet) destabilised
// Atmosphère on 22.5.0, so every public function below acquires the session,
// does its work and releases it before returning — including on error paths.
//
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#pragma once
#include "nx_types.h"
#include "pure.h"

typedef enum {
    PctlSafetyLevel_None       = 0,
    PctlSafetyLevel_Custom     = 1,
    PctlSafetyLevel_YoungChild = 2,
    PctlSafetyLevel_Child      = 3,
    PctlSafetyLevel_Teen       = 4,
} PctlSafetyLevel;

// Mirror of libnx's PctlRestrictionSettings (3 bytes on the wire). Kept under a
// different name so this header also builds where libnx is absent.
typedef struct {
    u8   rating_age;                       // 0 == no age restriction
    bool sns_post_restriction;             // true == posting screenshots to social media is restricted
    bool free_communication_restriction;   // true == communicating with other players is restricted
} PctlCustomSettings;

// ---- session ----
Result pctl_ops_init(void);    // idempotent; marks ownership only on success
void   pctl_ops_exit(void);    // releases only this layer's reference
Result pctl_ops_reinit(void);  // release, then acquire once (recovers a session the sysmodule closed)

// ---- overall status ----
// Every field has an *_ok flag: false means "that query failed / is not
// available on this firmware" — show "unavailable", never a default value.
typedef struct {
    Result session_rc;

    bool safety_level_ok;          u32  safety_level;            // 1032
    bool pin_length_ok;            u32  pin_length;              // 1206 (0 == no PIN)
    bool restriction_enabled_ok;   bool restriction_enabled;     // 1031
    bool temp_unlocked_ok;         bool temp_unlocked;           // 1006
    bool pairing_active_ok;        bool pairing_active;          // 1403
    bool settings_ok;              PctlCustomSettings settings;  // 1035
    bool rating_org_ok;            u32  rating_org;              // 1037
    bool stereo_vision_ok;         bool stereo_vision_restricted;// 1062 [4.0.0+]
    bool free_comm_count_ok;       u32  free_comm_count;         // 1039
    bool last_updated_ok;          u64  last_updated;            // 1406 (POSIX seconds)
} PctlStatus;

void pctl_status_fetch(PctlStatus *out);

const char *pctl_safety_level_name(u32 level);       // English fallback names (UI uses i18n)
const char *pctl_rating_org_name(u32 org);           // "PEGI", "ESRB", … or "?"

// ---- PIN / unlock ----
Result pctl_set_pin(void);                   // opens the system PIN applet (registers / changes it)
// The system's PIN screen (pctlauth, as System Settings shows it), to confirm
// it is the parent: 0 when the right PIN was entered, NXM_RC_NO_PIN (nothing
// shown) when none is set, anything else when it was cancelled. Changes
// nothing, so read-only mode and the change check do not apply.
Result pctl_ask_pin(void);

// UnlockRestrictionTemporarily (1201) using the stored PIN read via GetPinCode
// (1208), then verifies IsRestrictionTemporaryUnlocked (1006). Returns
// NXM_RC_UNLOCK_NOT_EFFECTIVE when 1201 succeeded but 1006 still reads false.
// When 1006 itself fails after 1201, locks again (1007) and returns that error.
Result pctl_unlock_restriction_temporarily(void);
// GetPinCode (1208), for the "Show PIN" action: copies the stored PIN (4 to 8
// digits, NUL-terminated) into `out`. `out` is zeroed first and stays zeroed
// on any error. The caller wipes it after use; never log or report it.
Result pctl_get_pin(char *out, size_t out_size);
// RevertRestrictionTemporaryUnlocked (1007): re-applies restrictions now.
Result pctl_relock(void);

// ---- destructive ----
Result pctl_delete_parental_controls(void);  // 1043: wipes the PIN and every restriction — CANNOT BE UNDONE
Result pctl_delete_pairing(void);            // 1941: unlinks the companion phone app

// ---- restrictions ----
Result pctl_set_safety_level(u32 level);                       // 1033
Result pctl_set_custom_settings(const PctlCustomSettings *s);  // 1036 (only when level == Custom)
Result pctl_set_stereo_vision_restricted(bool restricted);     // 1063 [4.0.0+]

// ---- play timer ----
// Per-day value sentinel: no configured limit at all for that day.
// (0 is a configured 0-minute limit; the two are different.)
#define PT_DAY_NOLIMIT 0xFFFFu

typedef struct {
    bool   session_valid;  Result session_rc;
    bool   fw_supported;   // false below 21.0.0: nothing else is filled in

    bool   valid;                 Result config_rc;    // 145601 GetPlayTimerSettings
    u16    day_min[7];            // Sun..Sat minutes or PT_DAY_NOLIMIT; meaningful only when valid
    u16    block[PT_U16_COUNT];   // the 0x44 block as read (backups keep it; pure.h)
    bool   enabled_valid;         Result enabled_rc;     bool enabled;     // 1453
    bool   restricted_valid;      Result restricted_rc;  bool restricted;  // 1455
    bool   temporary_unlocked_valid; Result temporary_unlocked_rc; bool temporary_unlocked; // 1006
    bool   remaining_valid;       Result remaining_rc;   u64  remaining_ns;// 1454
    bool   alarm_disabled_valid;  bool alarm_disabled;                     // 1458
    bool   bedtime_valid;         bool bedtime_enabled;  u8 bedtime_hour, bedtime_minute; // 1954/1956/1957
    bool   bedtime_reset_valid;   u8   bedtime_reset_hour, bedtime_reset_minute;          // 1958/1959 [20.0.0+]
} PtState;

void pctl_play_timer_query(PtState *out);

// The Overview's periodic read, in one session: of PctlStatus only safety
// level, PIN length, restriction enabled, temporarily unlocked and pairing;
// of PtState everything but the alarm flag (1458) and the bedtime reset time
// (1958/1959). 1006 is read once for both. What is not read stays *_ok false.
void pctl_overview_fetch(PctlStatus *status, PtState *pt);

// days_min[0]=Sunday .. [6]=Saturday. If every day is PT_DAY_NOLIMIT the timer is
// turned off. Re-checks 1453/1455/1006 in its own session right before writing
// and refuses (NXM_RC_WRITE_GATED) when the timer is active and not unlocked.
// Reads the current settings (145601) first and changes only the per-day limits,
// so what the companion app set in fields this app does not decode is kept.
Result pctl_play_timer_set_days(const u16 days_min[7]);
Result pctl_play_timer_set_uniform(u16 minutes);
Result pctl_play_timer_clear(void);

// Advanced (debug-class commands; exposed behind the "advanced" switch).
Result pctl_play_timer_set_alarm_disabled(bool disabled);  // 1953
Result pctl_play_timer_start(void);                        // 1451
Result pctl_play_timer_stop(void);                         // 1452

// Writes a multi-line read-only diagnostic report (raw rc + bytes for every
// command above plus 1459/1460/1952/1960). PIN contents are never included.
void pctl_dump(char *buf, size_t bufsz);
