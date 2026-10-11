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
| `04..0B` | `00 00 00 06 00 00 00 00` | the same format as a day; probably the `DAILY` rule (hypothesis below). Its `06` (byte `07`) stays in the "off" block, but was seen turn to `00` with the limits kept (observed, 22.0.0, between 20:10 and 20:44 on 2026-10-09; what changed it is not known) |

The byte-by-byte reading of a real block is under
[Reading a diagnostic report](#reading-a-diagnostic-report).

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

What PlayGuard accepts as this layout (`pt_plausible`, `source/core/pure.c`):
in each day and in the header's rule `04..0B`, the bedtime and limit
switches `0` or `1`, hours below 24, minutes below 60, the limit 0 to 1440
minutes (or `FFFF`). The header's four mode bytes are not decoded, so any
value passes there. Only what the
layout cannot hold is refused, so an unseen companion-app setting does not
block PlayGuard. A block that fails (a firmware that changed the layout, or
garbage) is not shown, and every play-timer write refuses it without writing
(*the console's play-timer settings are in a format PlayGuard does not
understand*); the diagnostic report still has it, byte for byte.

How the minutes field was confirmed (22.0.0, 2026-10-09): the block was saved
as a reference (Developer tools › play-timer block), Sunday and Saturday were
changed from 120 to 180 minutes in PlayGuard, then the block was read again.
Only two bytes changed: `0x12` and `0x42`, both `78` → `B4`. The header, the
flags and the bedtime bytes stayed as they were.

### Hypothesis: the header is the modes plus a "daily" rule

**Not verified.** The companion app's API, as reproduced by open-source
clients ([pynintendoparental](https://github.com/pantherale0/pynintendoparental),
used by Home Assistant, and
[switch-parental-controls](https://github.com/udondan/switch-parental-controls)),
describes the play timer with:

- `restrictionMode`: `ALARM` (an alarm only) or `FORCED_TERMINATION` (the
  software is suspended);
- `timerMode`: `DAILY` (one rule for every day) or `EACH_DAY_OF_THE_WEEK`;
- `dailyRegulations`: that one rule, and `eachDayOfTheWeekRegulations`: seven,
  each with a limit and a bedtime.

That is exactly eight rules and a few modes, and the block holds:

- 12 header bytes = **4 mode bytes + one 8-byte rule**;
- bytes `04..0B` follow the day format to the byte, down to the `06` of the
  default 06:00 "allowed again" time at their +3, which no other header field
  would explain;
- that `06` stays when everything else is cleared, like a default. It was
  also seen turn to `00` while every limit stayed: a value that can change,
  as an "allowed again" hour of a rule nobody uses might.

So the guess is: bytes `00..03` hold the modes (among them most likely
`timerMode`, `01` = each day of the week, since every observed block uses the
per-day rules, and `restrictionMode`), and bytes `04..0B` hold the `DAILY`
rule. Which mode sits in which byte is open.

How to test it, once a tool can write single bytes while the console is
temporarily unlocked (with the block saved first and put back after):
put a limit in the `04..0B` rule, set the byte that looks like `timerMode` to
0, then read 1454 / 1459: the remaining time should follow the daily rule.
For `restrictionMode`, let the time run out once with each value and note
whether the software is suspended.

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

**Changing the clock resets the time spent** (observed, 22.0.0,
2026-10-09, the evening reports):

| When (user clock, local) | What happened | 1952 spent | 1454 left |
|---|---|---|---|
| 20:10 | | 5772 s | 1428 s |
| between | automatic correction turned on: the user clock jumps 1411 s **forward**, to the network clock | | |
| 21:08 | | **54 s** | 7146 s |
| 21:08 → 20:45 | PlayGuard sets the network clock from NTP: the user clock follows, 1387 s **back** | | |
| 20:45:14 | | **0 s** | 7200 s |
| 20:45:27 | | 0 s | 7200 s |
| 20:46:22 | PlayGuard open the whole time | **0 s** | 7200 s |

- After the forward jump the day started over: 54 s counted since, out of
  more than 1900 s of reports apart. Whether the jump itself or something
  else in between reset it is inferred, not certain.
- The backward jump reset it at once, and the time then **stopped counting**
  (55 s with PlayGuard open, still 0). Likely explanation, to be confirmed:
  the console counts from a reference time that is now in the future (21:08)
  and waits for the clock to pass it.
- Both reset the remaining time to the full limit: changing the clock is a
  way around the play timer, and PlayGuard's own *Network clock* tab does it.

When the time spent resets on its own (midnight local time, or the "allowed
again" time) is not confirmed yet.

What PlayGuard does with this:

- **Timer health** (`source/action/timer_health_logic.cpp`): since its own
  time counts while it runs as an application, the time left must go down
  while it is on screen. With the timer on, a limit above 0 today, not
  reached, not temporarily unlocked and PlayGuard not opened from the album,
  a time left that has not moved by 5 s over about 90 s on screen makes the
  Overview and the Play timer say the console is not counting. The likely
  causes it gives come from above (a clock set back) and from other tools'
  users ([NX-Pctl-Manager #2](https://github.com/tailiang2008/NX-Pctl-Manager/issues/2),
  a network clock never set; #3 and #7, a limit just written). Not seen on a
  console yet: inferred from the table above.
- **Changes made outside PlayGuard** ([config.md](config.md#watchjson)): the
  time spent, taken as today's limit minus the time left (they add up to the
  limit), going down on the same day; the user clock moving against the
  steady clock; limits that differ from the ones PlayGuard last saw.

### When the time runs out

**A limit written by PlayGuard is enforced by the console** (verified,
22.0.0, 2026-10-09): once today's time was used up, the system put up its
full-screen "time limit reached, no more play today" message (in French:
*Vous avez atteint la limite de temps de jeu et ne pouvez plus jouer
aujourd'hui*), with two choices only: *Sleep mode* and *Disable parental
controls* (the second asks for the PIN).

- No "continue" choice: this looks like the suspend behaviour
  (`FORCED_TERMINATION`) rather than an alarm only (inferred; which header
  byte selects it is still open).
- So PlayGuard says what the console does at the limit without promising the
  suspension: "Time's up" always; the game suspended on the console it was
  tested on; with a phone app once set to "alarm only", maybe a warning only,
  a setting PlayGuard cannot read or change yet. Suspending can lose unsaved
  progress (Nintendo: [time limit settings](https://www.nintendo.com/sg/parents/switch/time/settings.html)),
  so its confirmations say so, and *No more play today* can wait 5 minutes.
- The PIN prompt behind *Disable parental controls* is the system's: a child
  who knows the PIN gets past the limit there.

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
- On the test console the network clock was 1411 s (23.5 min) ahead of the
  user clock, with automatic correction off and accuracy insufficient
  (observed, 22.0.0, 2026-10-09). The network clock was the wrong one: once
  automatic correction was turned on, the user clock jumped to it, and
  setting the network clock from NTP (europe.pool.ntp.org, then
  fr.pool.ntp.org) put both back 1387 s, verified by reading it back. 200
  then read `true`.
- With automatic correction on, the user clock follows a network clock
  written by PlayGuard immediately (same value in the report taken right
  after).

## Reading a diagnostic report

The report prints what each command returns, raw. Three habits make it
readable:

- **Little-endian.** Every number the console sends is stored lowest byte
  first. `78 00` is `0x0078` = 120; `00 C8 6E 7B 4C 01 00 00` is
  `0x0000014C7B6EC800`.
- **TimeSpan is in nanoseconds.** Divide by 1 000 000 000:
  `0x0000014C7B6EC800` = 1 428 000 000 000 ns = 1428 s = 23 min 48 s.
- **`rc=0x00000000` is success.** Anything else is a Horizon result code; the
  value after it is then meaningless (the report prints `-`).

### A free-communication list entry (1044)

```
00 60 CE 0E A0 C9 00 01   01   00 00 00 00 00 00 00
└──────── u64 LE ───────┘  │   └─────── zeros ─────┘
 application 0100C9A00ECE6000  unknown, always 01
```

### The play-timer block, byte by byte

The block of the 20:10 report (22.0.0, 2026-10-09), as the report prints it:

```
01 01 01 00 00 00 00 06 00 00 00 00 00 00 00 06
00 01 B4 00 00 00 00 06 00 01 00 00 00 00 00 06
00 01 00 00 00 00 00 06 00 01 00 00 00 00 00 06
00 01 00 00 00 00 00 06 00 01 78 00 00 00 00 06
00 01 B4 00
```

Cut into its fields instead of rows of 16:

```
offset  bytes                     field
0x00    01 01 01 00               header: four mode bytes (see the hypothesis below)
0x04    00 00 00 06 00 00 00 00   an eighth rule? same format as a day, see below
0x0C    00 00 00 06 00 01 B4 00   Sunday     limit on, 0x00B4 = 180 min
0x14    00 00 00 06 00 01 00 00   Monday     limit on, 0 min: no play
0x1C    00 00 00 06 00 01 00 00   Tuesday    limit on, 0 min
0x24    00 00 00 06 00 01 00 00   Wednesday  limit on, 0 min
0x2C    00 00 00 06 00 01 00 00   Thursday   limit on, 0 min
0x34    00 00 00 06 00 01 78 00   Friday     limit on, 0x0078 = 120 min
0x3C    00 00 00 06 00 01 B4 00   Saturday   limit on, 0x00B4 = 180 min
```

One day, field by field (Sunday above):

```
00   00   00   06   00   01   B4 00
│    │    │    │    │    │    └───┴── limit, minutes, u16 LE: 0x00B4 = 180   verified
│    │    │    │    │    └─────────── limit flag: 1 = this day has a limit    verified
│    │    │    │    └──────────────── play allowed again, minute: 00          inferred
│    │    │    └───────────────────── play allowed again, hour: 06            inferred
│    │    └────────────────────────── bedtime alarm, minute                   inferred
│    └─────────────────────────────── bedtime alarm, hour                     inferred
└──────────────────────────────────── bedtime on                              inferred
```

The code (`source/core/pure.h`) reads the same block as 34 u16 instead of
bytes. The `header=0101 0001` of the report is the first two: bytes `01 01`,
then `01 00`. Because the header is 12 bytes, a day's u16 straddle its fields:
a day's `[+0]` = `0x0600` is its bytes +2 and +3 (alarm minute, allowed-again
hour), `[+1]` = `0x0100` its bytes +4 and +5 (allowed-again minute, limit
flag), `[+2]` its minutes.

The morning reports (no PIN, no limit) show the "everything off" block:

```
0x00    00 00 00 00               header: all zero
0x04    00 00 00 06 00 00 00 00   only the 06 is left
0x0C    00 00 00 00 00 00 00 00   Sunday … Saturday: all zero
 …
```

### 1459, byte by byte

From the same 20:10 report:

```
0x00  02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
      └ 02: today has a limit (00 without one; other values unknown)
0x10  00 C8 6E 7B 4C 01 00 00 00 00 00 00 00 00 00 00
      └──────── u64 LE ───────┘
       0x0000014C7B6EC800 ns = 1428 s, the value 1454 returned
```

## Still open

- 1406 `GetSettingsLastUpdated` fails with `0x0001188E`: why?
- The header: which of bytes `00..03` is `timerMode`, which
  `restrictionMode`; whether `04..0B` is the `DAILY` rule (hypothesis above).
- The bedtime bytes with a bedtime actually on.
- 1459's first byte; 1460's layout (23.0.0+).
- When the time spent resets on its own; how long it stays frozen after the
  clock goes back (until the clock passes the old time?).
- What turned the header's byte `07` from `06` to `00`.
- The `01` byte of each free-communication list entry.
- Whether time spent counts while temporarily unlocked (the app says it
  *may* not).
- Whether a game left open on the HOME menu counts (parents report it does;
  it fits "while an application is open") and sleep mode does not: not
  measured by PlayGuard.
- Whether a PIN entered from the HOME menu is logged anywhere PlayGuard can
  read (the companion app shows it since 2.5.0; see
  [companion-app.md](companion-app.md)).

## How to add a finding

1. Turn on the developer tools, take a diagnostic report, change one thing,
   take another one. For the play-timer block, save the reference first
   (Developer tools › play-timer block): the next report lists every byte that
   changed.
   For what changes over time (the time spent, 1459, a reset at midnight),
   turn on *Developer tools › Record the play timer* instead: one CSV line
   every 30 s in `logs/play_timer_log.csv` while PlayGuard is open (its own
   time counts, so the time runs down with PlayGuard alone). The two hex
   columns are written only when they change. The last column, `event`, says
   why a line was written off the 30 s tick: `recording started`, each change
   PlayGuard makes, as its change history words it (`limits per_day [120 …]
   -> [180 …]`, `clock`, `unlock` …), written right after it, and
   `clock +1411 s vs elapsed` when the user clock moved more than the time
   that went by (5 s of slack): a clock changed in System Settings, or the
   console asleep in between. What is done outside PlayGuard (System
   Settings, the PIN typed in the HOME menu) is not named: note it down. Its last 48 KB go with *Send a
   report online*. PlayGuard does not run in the background: the recording
   stops while a game is open, and while the console sleeps.
2. Note the firmware, the date and exactly what was changed between the two.
3. Add the fact here with its status, and update the comment next to the code
   that uses it.
