# PlayGuard and the companion app

PlayGuard brings the settings of Nintendo's companion app,
**Nintendo Switch Parental Controls**
([App Store](https://apps.apple.com/app/id1190074407),
[Google Play](https://play.google.com/store/apps/details?id=com.nintendo.znma)),
onto the console itself. A console running Atmosphère is usually kept away
from Nintendo's servers, so the companion app can no longer reach it: every
setting it offers should be reachable from PlayGuard, offline.

This page lists what the companion app does (version 2.6.0, September 2026,
from its store page and release notes), what PlayGuard already covers, and
what is still missing. It is the to-do list for parity: when a gap closes,
move it to the first table.

How the settings are stored on the console is in
[parental-controls.md](parental-controls.md).

## Already covered

| Companion app | PlayGuard | Notes |
|---|---|---|
| Daily play-time limit, the same every day or one per day | *Play timer* | Plus saved profiles, and the week chart as the editor |
| "Extend today's play time" (+5 min … +1 h, added in 2.4.0 for +1 h); undo today's changes | *Extra time today*, *No more play today*; the usual limit comes back the next day | Amounts in *Preferences*, including +5/+10/+15 |
| Bedtime alarm and the time play is allowed again (2.1.0) | *Play timer › Bedtime alarm* | Partly: one time for every day (see gap 2) |
| Restriction level, age rating, rating organisation, posting to social media, communicating with other players, VR mode (1.10.0) | *Restrictions* | |
| Play history per game | *Activity* | Plus per user account, all-time totals and export (CSV, JSON, XLSX, PDF) |
| Parental-controls PIN | *Security & app* | Plus showing the PIN (asked first) and a PIN to open PlayGuard itself; for a forgotten PIN, the recovery sysmodule |
| Linking the app to a console | *Security & app › Unlink the companion app* | PlayGuard can only unlink, which is what a console kept offline needs |

## Missing

Most useful first. Each says what blocks it.

### 1. Alarm only, or suspend the software

When the time is up, the companion app either sounds an alarm or suspends
the software. This is the setting parents care about most after the limit
itself.

- In the app's API it is `restrictionMode`: `ALARM` or `FORCED_TERMINATION`.
- On the console it most likely sits in the play-timer block's header, bytes
  `00..03`, but which byte and which value is **unknown**
  (see [the header hypothesis](parental-controls.md#hypothesis-the-header-is-the-modes-plus-a-daily-rule)).
- **Needs:** a console still linked to the companion app. Save the play-timer
  block as a reference (*Security & app › Before unlinking: help decode…*
  while linked, or *Developer tools › Compare the play-timer block*),
  switch the mode in the app, let it sync, compare. Attach the result to an
  issue.

### 2. Bedtime and start time per day of the week

Since 2.4.0 the companion app sets a different start time for each day
(*Detailed end-of-day settings*), and each day's rule has its own bedtime.
PlayGuard writes one bedtime, the same every day.

- The per-day bedtime bytes are inferred from the app's API but were **never
  seen with a bedtime on**; PlayGuard checks every bedtime write against what
  the console reports and reverts it otherwise.
- **Needs:** the same comparison as gap 1, with a different bedtime on two
  days. Then the per-day editor can show and set a bedtime per day.

### 3. Exceptions for specific software

The companion app can let a given game through the restriction level (for
example, allow communication in one game only). PlayGuard shows how many games
are on the free-communication list (command 1039) but not which ones.

- The list's entry layout is only partly known (an application ID, then a
  `01` byte whose meaning is open); the raw list is in the diagnostic report.
- **Needs:** a report with a known list (one or two games added from the
  app), to decode the entries. Then show them by name, then add and remove.

### 4. History of PIN entries

Since 2.5.0 the companion app shows when the PIN was entered on the console,
and can notify the parent.

- **Unknown** whether the console logs this in a place PlayGuard can read: the
  play-event log (`pdm:qry`, already read for *Activity*) or the
  parental-control service itself.
- **Needs:** a play-event log dump from a console where the PIN was entered,
  looking for an event at that time.
- What PlayGuard already has: its own change history (*Tools › Change
  history*) records its temporary unlocks, not those made from the HOME menu.

### 5. Monthly report

The companion app sends a monthly activity report.

- Nothing blocks it: *Activity* already reads the whole play-event log. It
  needs a month view (per game, per account, per day) and the same export.

## Out of scope

| Companion app | Why not |
|---|---|
| Push notifications, remote control, a second parent, up to eight consoles | PlayGuard runs on the console, offline, and nothing of it runs in the background |
| GameChat settings | Nintendo Switch 2 only, which Atmosphère does not run on |
| Purchase restrictions | Nintendo Account settings, on Nintendo's servers, not on the console |

## Asked for in the store reviews

Not in the companion app either; noted here because parents ask for them.

| Request | Status |
|---|---|
| A limit per user account | Not possible with the console's play timer: one limit for the whole console. A replacement sysmodule does it (see the README's comparison) |
| A warning a few minutes before the end, on the console | The console's own warnings; PlayGuard does not change them |
| A start time before which the console cannot be used | Covered by the bedtime's "allowed again" time; per day is gap 2 |
| Settings changed while the console is off, applied later | Not applicable: PlayGuard changes the console directly |
