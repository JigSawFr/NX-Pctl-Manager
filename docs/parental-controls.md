# How the Switch parental controls work

What PlayGuard knows about the console's parental-control service (`pctl`),
the play timer and the clocks around it: what the public references say, and
what was found on hardware while building PlayGuard. It is meant to grow:
every diagnostic report that teaches something new should end up here.

Each fact says how sure it is:

- **verified**: seen on a console, with the firmware it was seen on;
- **observed**: seen in diagnostic reports, consistent, but not tried every way;
- **inferred**: deduced from the layout or from other facts, not tested directly;
- **unknown**: still open. A diagnostic report that answers it is welcome.

The code is the reference for the details: `source/core/pctl_ops.c` (command
table, session handling), `source/core/pure.h` (the play-timer block),
`source/core/time_ops.h` (clocks), `sysmodule/source/main.c` (rescue).
Public reference: [switchbrew, Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).

## The service and its session

- PlayGuard runs under Atmosphère, where libnx's `pctlInitialize` lands on a
  privileged `pctl:a` / `pctl:s` session. The privileged commands
  (settings, PIN, play timer writes) only work there.
- `pctl:a` takes **a single session**. Holding it while the system wants it
  (the HOME menu's PIN prompt, the `pctlauth` applet) made Atmosphère unstable
  (verified, 22.5.0). PlayGuard therefore opens a session, does one piece of
  work and closes it, every time, including on errors.
- The PIN applet (`pctlauth`, used to register, change or ask for the PIN)
  opens its own privileged session: PlayGuard's must be closed first.
- A malformed request makes the service drop the session (`0xF601`): the next
  command needs a fresh one (`pctl_ops_reinit`).

## Restrictions

| Command | What | Notes |
|---|---|---|
| 1031 `IsRestrictionEnabled` | parental controls on | `false` while no PIN is set (observed, 22.0.0) |
| 1032 / 1033 `Get/SetSafetyLevel` | 0 none, 1 custom, 2 young child, 3 child, 4 teen | |
| 1034 `GetSafetyLevelSettings(level)` | what a preset restricts | 22.0.0: level 2 → age 7, sns 1, comm 1; level 3 → 12, 1, 1; level 4 → 16, 0, 0 (observed) |
| 1035 / 1036 `GetCurrentSettings` / `SetCustomSafetyLevelSettings` | 3 bytes: rating age, SNS posting restricted, free communication restricted | 1036 only applies at the custom level |
| 1037 / 1038 `Get/SetDefaultRatingOrganization` | rating body, < 13 | 6 is PEGI on a European console (observed) |
| 1039 / 1044 free-communication application list | count, then 16-byte entries | entry: application ID (u64, LE), then `01` and seven zeros (observed); the meaning of the `01` is unknown |
| 1062 / 1063 stereo vision (3D) restriction | bool | follows the temporary unlock: `false` while unlocked, `true` once locked again (observed, 22.0.0) |
| 1903 / 1904 exempt application list (debug) | count, then entries | always empty so far |
| 1406 `GetSettingsLastUpdated` | POSIX time | **always fails with `0x0001188E`** on the test console (observed, 22.0.0); the cause is unknown |
| 1403 `IsPairingActive` | companion app paired | |
| 1043 `DeleteSettings` | wipes the PIN and every restriction | **cannot be undone** |
| 1941 `DeletePairing` | unlinks the companion app | |
| 1601 / 1602 / 1603 | | return `0x00010A8E` on a normal console: not usable |

## PIN and temporary unlock

- 1206 `GetPinCodeLength`: 0 means no PIN.
- 1208 `GetPinCode`: the PIN comes back in an **HIPC pointer buffer**
  (`SfBufferAttr_HipcPointer`), NUL-terminated. PlayGuard never logs or reports
  its content.
- 1201 `UnlockRestrictionTemporarily` (verified, 22.1.0), two traps:
  - the buffer must be an HIPC pointer buffer; a map-alias buffer, or no
    buffer, makes the service drop the session (`0xF601`);
  - the PIN must be passed NUL-terminated (`length + 1` bytes); the bare digits
    are refused with `0xF80E`.
- 1006 `IsRestrictionTemporaryUnlocked` confirms the unlock. 1201 can succeed
  while 1006 still reads `false`: PlayGuard checks, and locks again with 1007
  `RevertRestrictionTemporaryUnlocked` when it cannot tell.
- While temporarily unlocked, 1453 `IsPlayTimerEnabled` reads `false`
  (observed, 22.0.0).
- Registering or changing the PIN goes through the `pctlauth` applet; the
  direct `SetPinCode` command is refused.
- The rescue sysmodule uses the same 1208 → 1201 sequence at boot, with no
  applet and no screen, so it still works while the play timer blocks every
  program.

## The play timer

### Commands

| Command | What | Notes |
|---|---|---|
| 1453 `IsPlayTimerEnabled` | timer on | `true` even with an all-zero block and no PIN; `false` while temporarily unlocked (observed, 22.0.0) |
| 1455 `IsRestrictedByPlayTimer` | the timer is blocking play now | |
| 1454 `GetPlayTimerRemainingTime` | TimeSpan, **nanoseconds** | today's limit minus the time spent; 0 when today has no limit |
| 1952 `GetPlayTimerSpentTimeForTest` | TimeSpan, nanoseconds | time spent today, see below |
| 1459 `GetPlayTimerRemainingTimeDisplayInfo` [20.0.0+] | 0x20 bytes | layout partly known, see below |
| 1458 / 1953 alarm disabled (get / set for debug) | bool | while set, the "time's up" alarm does not show |
| 1451 / 1452 `Start/StopPlayTimer` | | |
| 1954 `IsBedtimeAlarmEnabled` [18.0.0+] | bool | today's bedtime |
| 1956 / 1957 bedtime alarm hour / minute [18.0.0+] | u8 each | |
| 1958 / 1959 bedtime "allowed again" hour / minute [20.0.0+] | u8 each | |
| 1960 `GetExtraPlayingTimeForDebug` [20.0.0+] | | always 0 so far |
| 145601 `GetPlayTimerSettings` | 0x44 bytes [21.0.0+ size] | the block below |
| 195101 `SetPlayTimerSettingsForDebug` | 0x44 bytes | writes the block |
| 1460 `GetWatcherStatusDisplayInfo` [23.0.0+] | in 1 byte, out 0x18 bytes | layout unknown |

**Never call 1456 or 1951**: they are the 18.0.0–20.5.0 "old" variants and
close the session on 21.0.0+.

### Writing safely

Writing the settings while the timer counts down destabilises Atmosphère.
Every PlayGuard play-timer write re-reads 1453 / 1455 / 1006 in its own
session right before writing, and refuses while the timer is enabled or
restricting and not temporarily unlocked.

### The settings block (145601 / 195101)

0x44 bytes: a 12-byte header and seven 8-byte days, Sunday to Saturday.

```
offset  size  field
0x00    12    header
0x0C     8    Sunday
0x14     8    Monday
 ...
0x3C     8    Saturday
```

Header, as seen so far:

| Bytes | Value | Meaning |
|---|---|---|
| `00 01` | `01 01` | non-zero while a limit or a bedtime is set (observed) |
| `02 03` | `01 00` | unknown, constant |
| `04..06` | zeros | unknown |
| `07` | `06` | unknown; there even in the "off" block, with every other byte zero (observed) |
| `08..0B` | zeros | unknown; with `04..06`, candidates for the "alarm only" vs "suspend the software" choice and a daily mode |

One day (8 bytes):

| Byte | Field | Status |
|---|---|---|
| +0 | bedtime on | inferred |
| +1, +2 | bedtime alarm hour, minute | inferred |
| +3, +4 | play allowed again, hour, minute (06:00 by default) | inferred; the `06` is always there |
| +5 | "this day has a limit" flag | verified |
| +6, +7 | that day's limit in minutes, u16 LE | **verified, 22.0.0** |

- A day with no limit: flag 0, minutes 0. A day with its flag on and 0 minutes
  is a real **0-minute limit**: no play that day. PlayGuard writes it for a day
  set to 0.
- Everything zero but the header's byte `07` (`06`): no limit and no bedtime.
  PlayGuard writes all zeros for "no limit on any day", the one "off" block
  seen working. 1453 still reads `true` with that block (observed, 22.0.0).
- The bedtime bytes match the companion app's daily settings
  (`timeToPlayInOneDay`, `bedtime.enabled`, `endingTime`, `startingTime`), but
  were never seen with a bedtime on. A bedtime write is checked against
  1954 / 1956 / 1957 and undone when the console disagrees.

How the minutes field was confirmed (22.0.0, 2026-10-09): the block was saved
as a reference (Developer tools › play-timer block), Sunday and Saturday were
changed from 120 to 180 minutes in PlayGuard, then the block was read again.
Only two bytes changed: `0x12` and `0x42`, both `78` → `B4`. The header, the
flags and the bedtime bytes stayed as they were.

### Time spent and time left

Observed on 22.0.0, 2026-10-09 (a Friday, limit 120 minutes):

- 1454 + 1952 = today's limit, every time: 1645 + 5555, 1560 + 5640 and
  1428 + 5772 seconds, all 7200 s = 120 min.
- Changing other days' limits leaves today's remaining time alone.
- 1952 advances in real time while an application runs, **PlayGuard
  included**: +85 s between two reports 85 s apart, +132 s for 132 s.
- It also counts with no limit set and no PIN (+370 s in 370 s, every day
  empty).
- It only grew by 354 s across two hours, part of them temporarily unlocked:
  time outside applications, or while unlocked, does not seem to count
  (inferred; not measured separately).

When the time spent resets (midnight local time, or the "allowed again" time)
is not confirmed yet.

### 1459 `GetPlayTimerRemainingTimeDisplayInfo`

0x20 bytes. Observed on 22.0.0:

| Offset | Size | Value |
|---|---|---|
| 0x00 | 1 | `02` while today has a limit, `00` without one |
| 0x01..0x0F | | zeros |
| 0x10 | 8 | the remaining time in nanoseconds, the same value as 1454 |
| 0x18..0x1F | | zeros |

Without any limit, all 0x20 bytes are zero. What the first byte means (a
display state? which message the HOME menu shows?) is **unknown**: a report
taken with the time used up, or a few minutes before, should tell.

## Clocks

The play timer counts per day, so the console's clocks matter.

- The `time:s` service gives the user clock (what the HOME menu shows), the
  network clock and the local clock, in UTC POSIX seconds.
- 100 `IsStandardUserSystemClockAutomaticCorrectionEnabled`: when it is off,
  the user clock does not follow the network clock. PlayGuard refuses to write
  the network clock then, so the system's own setting stays in charge.
- 200 `IsStandardNetworkSystemClockAccuracySufficient`.
- On the test console the network clock stays 1411 s (23.5 min) ahead of the
  user clock, with automatic correction off and accuracy insufficient
  (observed, 22.0.0, every report of 2026-10-09).

## Still open

- 1406 `GetSettingsLastUpdated` fails with `0x0001188E`: why?
- The header bytes `02..0B`: where the "alarm only" vs "suspend the software"
  choice is kept.
- The bedtime bytes with a bedtime actually on.
- 1459's first byte; 1460's layout (23.0.0+).
- When the time spent resets.
- The `01` byte of each free-communication list entry.
- Whether time spent counts while temporarily unlocked.

## How to add a finding

1. Turn on the developer tools, take a diagnostic report, change one thing,
   take another one. For the play-timer block, save the reference first
   (Developer tools › play-timer block): the next report lists every byte that
   changed.
2. Note the firmware, the date and exactly what was changed between the two.
3. Add the fact here with its status, and update the comment next to the code
   that uses it.
