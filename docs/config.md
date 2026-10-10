# `config.json`

PlayGuard's settings, in `sd:/switch/playguard/config.json`. Everything here is
set from the app; the file is only worth editing by hand to get back in (see
[`pin_lock`](#pin_lock)) or to check what PlayGuard remembered.

The code is the reference: `source/util/config.hpp` (fields and defaults) and
`source/util/config.cpp` (reading, checks, writing). The remote link's
settings are not here but in `sync.conf` ([`sync-protocol.md`](sync-protocol.md)).

## How the file is read

- **A missing file, or one that is not JSON, gives the defaults.** PlayGuard
  writes the whole file again at the next change.
- **Each key is read on its own.** A missing key keeps its default; a key of
  the wrong type keeps its default too, and the others are still read.
  Types are strict: `true`, not `1`; `90`, not `90.5` or `"90"`.
- **A value outside the list below goes back to its default**, without a
  message.
- Keys PlayGuard does not know are ignored, and dropped at the next save.
- Some groups of keys are all-or-nothing: the [firmware choice](#firmware-not-supported-yet)
  and the [extra time of the day](#extra-time-today). One key refused, and the
  whole group is cleared.
- The file is written atomically (a temporary file, then a rename), indented
  with two spaces.

## Preferences

| Key | Type | Values | Default | Set in |
|---|---|---|---|---|
| `schema` | integer | `1` | `1` | written by PlayGuard, not read |
| `language` | string | `"system"` (the console's language), `"en-US"`, `"fr"`, `"fr-CA"`, `"de"`, `"es"`, `"es-419"`, `"it"`, `"nl"`, `"pt"`, `"pt-BR"`, `"ru"`, `"ja"`, `"ko"`, `"zh-Hans"`, `"zh-Hant"` | `"system"` | Preferences |
| `theme` | string | `"system"`, `"light"`, `"dark"` | `"system"` | Preferences |
| `start_tab` | string | the tab shown at start-up: `"dashboard"` (Overview), `"play_timer"`, `"activity"`, `"restrictions"`, `"clock"` (Network clock), `"security"`, `"preferences"`, `"tools"`, `"about"` | `"dashboard"` | Preferences |
| `advanced` | boolean | shows the play timer's debug-class actions | `false` | Preferences |
| `auto_relock` | boolean | locks parental controls again right after a change that needed the temporary unlock | `true` | Preferences |
| `onboarding_at_start` | boolean | *First steps* opens by itself at start-up while no PIN is set | `true` | First steps (*Show at start-up*) |
| `clock_check_at_start` | boolean | measures the network clock at start-up and says when it is off | `false` | Preferences |
| `activity_period` | integer | Activity's period: `0` today, `1` last 7 days, `2` all time | `1` | Activity |
| `export_format` | integer | the last export format: `0` CSV, `1` JSON, `2` XLSX, `3` PDF | `0` | Activity › Export |
| `backup_keep` | integer | backups kept after a new one: `0` (all of them), `5`, `10`, `20` | `0` | Tools |
| `extra_amounts` | array of 3 integers | the *Extra time today* choices, in minutes: `[15, 30, 60]`, `[10, 20, 30]`, `[30, 60, 90]` or `[5, 10, 15]` | `[15, 30, 60]` | Preferences |

## `pin_lock`

*Security › Ask for the PIN*. A string:

| Value | In the app | What it does |
|---|---|---|
| `"off"` | Never | Nothing is asked. The default. |
| `"changes"` | Before a change | Anyone can look; the first change (or *Show the PIN*) asks for the PIN, then nothing is asked for 5 minutes. |
| `"open"` | To open PlayGuard | A lock screen first; the right PIN opens the app, B quits. |

- With no PIN on the console, nothing is asked whatever the value.
- In the app, a lower setting asks for the PIN first. The file itself is not
  protected: it keeps a child out of PlayGuard, not someone who edits the SD
  card.
- **PIN forgotten while this is `"changes"` or `"open"`:** put the SD card in a
  computer and set `"pin_lock": "off"`.

## Network clock

| Key | Type | Values | Default |
|---|---|---|---|
| `ntp_server` | string | the NTP server used; empty picks one from the console's region. At most 253 characters, or it is cleared | `""` |
| `custom_servers` | array of strings | servers added by hand: at most 10, each 1 to 253 characters (empty or longer ones are dropped) | `[]` |

## Updates

Set in *About*.

| Key | Type | Values | Default |
|---|---|---|---|
| `update_via` | string | how an update is installed: `"auto"` (Sphaira if installed, else the Homebrew App Store), `"sphaira"`, `"appstore"`, `"manual"` (only say an update exists) | `"auto"` |
| `update_daily` | boolean | checks for an update at start-up, once a day | `false` |
| `update_checked` | string | `"YYYY-MM-DD"` of the last check; anything not 10 characters long is cleared | `""` |

## Reminders

| Key | Type | Values | Default |
|---|---|---|---|
| `support_reminder` | boolean | *Support PlayGuard* once a month at start-up (Preferences) | `true` |
| `support_reminded` | string | `"YYYY-MM-DD"` of the last reminder; anything else is cleared | `""` |
| `seen_version` | string | the version whose *What's new* was shown; at most 32 characters | `""` |
| `agent_update_skipped` | string | the SHA-256 of the remote link agent PlayGuard carries that the offer to update the installed one was answered *Later* to: not offered again at start-up for that build (*Tools › Optional modules* still offers it); at most 64 characters | `""` |

## Developer tools

| Key | Type | Values | Default |
|---|---|---|---|
| `dev_mode` | boolean | the developer tools (seven presses on *About › Version*) | `false` |
| `pt_log` | boolean | *Developer tools › Record the play timer* (`logs/play_timer_log.csv`) | `false` |

## State kept between launches

Written by PlayGuard to finish what it started. Better left alone: a wrong
value here can make PlayGuard put back a limit nobody asked for.

### Console lock

*Security › Console lock*: every day's limit set to 0, so the PIN is needed to
play.

| Key | Type | Values | Default |
|---|---|---|---|
| `console_lock` | boolean | the console lock is on | `false` |
| `console_lock_prev` | array of 7 integers | the limits it replaced, Sunday to Saturday, in minutes: `0` to `1440`, or `65535` for no limit. Anything but seven valid values empties it; turning the lock off then clears the limits instead of putting them back | `[]` |

### Extra time today

*Extra time today* or *No more play today*: one day's limit changed for that
day only. All four keys go together.

| Key | Type | Values | Default |
|---|---|---|---|
| `extra_weekday` | integer | the day changed, `0` (Sunday) to `6`; `-1`: nothing pending | `-1` |
| `extra_date` | string | `"YYYY-MM-DD"` of the change | `""` |
| `extra_base` | integer | the limit before, in minutes: `0` to `1440`, or `65535` for no limit | `0` |
| `extra_value` | integer | the limit for that day, `0` to `1440` minutes | `0` |
| `extra_auto_restore` | boolean | the next day, put the limit back by itself instead of asking (the temporary unlock is still asked when needed); set in Preferences | `false` |

### Firmware not supported yet

The choice made on that screen, remembered for one firmware with one app
version. The three keys go together.

| Key | Type | Values | Default |
|---|---|---|---|
| `fw_gate_fw` | string | the firmware it applies to | `""` |
| `fw_gate_app` | string | the PlayGuard version it applies to | `""` |
| `fw_gate_choice` | string | `""` (none), `"read_only"`, `"probe"`, `"risk"` | `""` |

### Locking again

| Key | Type | Values | Default |
|---|---|---|---|
| `relock_pending` | boolean | set right before PlayGuard unlocks parental controls for a change, cleared once it has locked them again. Still `true` at start-up: the app stopped in between, so it locks again then | `false` |

## Example

A file as PlayGuard writes it, with *Ask for the PIN* set to *To open
PlayGuard*:

```json
{
  "schema": 1,
  "language": "fr",
  "theme": "system",
  "ntp_server": "",
  "custom_servers": [],
  "advanced": false,
  "auto_relock": true,
  "extra_auto_restore": false,
  "dev_mode": false,
  "pt_log": false,
  "update_via": "auto",
  "update_daily": false,
  "update_checked": "",
  "start_tab": "dashboard",
  "extra_amounts": [15, 30, 60],
  "activity_period": 1,
  "export_format": 0,
  "backup_keep": 0,
  "clock_check_at_start": false,
  "pin_lock": "open",
  "onboarding_at_start": true,
  "support_reminder": true,
  "support_reminded": "",
  "seen_version": "",
  "agent_update_skipped": "",
  "console_lock": false,
  "console_lock_prev": [],
  "fw_gate_fw": "",
  "fw_gate_app": "",
  "fw_gate_choice": "",
  "extra_weekday": -1,
  "extra_date": "",
  "extra_base": 0,
  "extra_value": 0,
  "relock_pending": false
}
```

A file with only `{"pin_lock": "off"}` is valid too: every other key keeps its
default.
