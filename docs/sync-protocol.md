# Remote link protocol

**Status: proposed, schema 1, nothing implemented.** The contract between the
console (PlayGuard and its optional agent), the MQTT broker, Home Assistant
and the `playguard` HA integration. The design and its reasons are in
[`sync-design.md`](sync-design.md). Until phase A lands, everything here may
change; once published, a change that breaks a consumer bumps `schema`.

Conventions: `<id>` is the console id (8 lowercase hex characters, random,
generated once, never the serial number). Times are POSIX seconds from the
console's user clock. Minutes are integers. A value the console could not read
is `null`, never a default. JSON consumers ignore unknown fields.

## Topics

| Topic | Retained | Direction | Content |
|---|---|---|---|
| `playguard/<id>/availability` | yes | console → | `online` or `offline`. Last will (`offline`), and `offline` published before a clean disconnect. |
| `playguard/<id>/state` | yes | console → | The state snapshot (below). |
| `playguard/<id>/activity` | yes | console → | Today's play time from the play log. |
| `playguard/<id>/activity/<YYYY-MM-DD>` | yes | console → | A finished day's totals, published at the first cycle after local midnight. The console keeps the last 14 and clears older ones (empty retained publish). |
| `playguard/<id>/names` | yes | console (app) → | Game and account names, published by PlayGuard when it runs. |
| `playguard/<id>/week` | yes | console (app) → | The 7-day table per game and per account, published by PlayGuard when it runs. |
| `playguard/<id>/<entity>/set` | yes (by the publisher) | → console | An order. Cleared by the console once handled. |
| `playguard/<id>/event` | **no** | console → | One event per line of history: applied, rejected, unlocked, limit reached… |
| `playguard/<id>/report` | yes | console → | The last diagnostic report, plain text, at most 128 KiB. Only when `publish_report` is on. |
| `homeassistant/device/playguard_<id>/config` | yes | console → | HA native discovery. Only when `ha_discovery` is on; cleared when it is turned off. |
| `homeassistant/status` | — | ← HA | Subscribed: on `online`, the console republishes discovery, state and activity. |

Client ids: `playguard-<id>-agent` for the agent, `playguard-<id>-app` for
PlayGuard running without the agent. Only one of them connects at a time.
Outbound publishes use QoS 0 (retained state heals itself at the next cycle);
the subscription to `playguard/<id>/+/set` and `homeassistant/status` uses
QoS 1. Keep-alive is 30 s, so a console that falls asleep is marked offline
within 45 s.

## `state`

```json
{
  "schema": 1,
  "source": "agent",
  "ts": 1760000000,
  "local_date": "2026-10-09",
  "clock_accurate": true,
  "console": {
    "id": "a1b2c3d4", "name": "Salon",
    "firmware": "23.0.1", "atmosphere": "1.12.0",
    "app_version": "1.1.0", "agent_version": "1.1.0",
    "emummc": true, "applet_mode": false, "read_only": false
  },
  "controls": {
    "enabled": true, "temp_unlocked": false, "pin_set": true,
    "level": "child", "rating_age": 12,
    "sns_post_restricted": true, "free_communication_restricted": true,
    "vr_restricted": false, "rating_org": 6,
    "companion_linked": false, "companion_last_sync": null
  },
  "timer": {
    "enabled": true, "running": true, "limit_reached": false,
    "limits_min": [180, 120, 120, 120, 120, 120, 180],
    "limit_today_min": 120, "remaining_min": 45, "used_min": 75,
    "spent_raw_min": 76,
    "alarm_disabled": false,
    "bedtime": { "enabled": true, "start": "21:00", "end": "06:00" },
    "extended_today_min": 0, "console_locked": false
  },
  "activity_today": {
    "used_min": 74,
    "now_playing": { "app_id": "0100000000010000", "since": 1759996400 }
  },
  "link": {
    "policy": "auto", "remote_timer_writes": false, "ha_discovery": true,
    "pending_orders": 0,
    "last_result": { "entity": "limit_mon", "applied": true, "rc": "0x00000000", "ts": 1759990000 }
  }
}
```

- `source`: `agent` or `app` (which process produced the snapshot).
- `clock_accurate`: the console reports its network clock as accurate (`time`
  command 200). `ts` is only trustworthy when it is true.
- `limits_min` runs Sunday to Saturday. **1440 means no limit**, on every
  topic and order, so that a plain number entity can express it.
- `used_min` is the console timer's own figure (`limit_today_min −
  remaining_min`); `spent_raw_min` is command 1952, unverified; both are `null`
  without an active limit. `activity_today.used_min` comes from the play log
  and exists even with no limit. See `parental-controls.md` for why they differ.
- `extended_today_min`: extra time granted today by PlayGuard or by an order.
- `console.read_only`: PlayGuard's runtime read-only mode (an untested
  firmware); every order is then refused.

## `activity`, `activity/<date>`, `names`, `week`

```json
{ "schema": 1, "source": "agent", "ts": 1760000000, "local_date": "2026-10-09", "final": false,
  "total_min": 74,
  "per_app": [ { "app_id": "0100000000010000", "min": 60 }, { "app_id": "01000A10041EA000", "min": 14 } ],
  "per_account": null,
  "now_playing": { "app_id": "0100000000010000", "since": 1759996400 } }
```

`activity/<date>` is the same document with `"final": true` and no
`now_playing`. `per_account` is filled by PlayGuard (the agent does not open
the account service). Application ids are 16 uppercase hex characters, as the
console prints them. PlayGuard's own time over a game is excluded, as in the
Activity tab.

```json
{ "schema": 1, "ts": 1760000000,
  "games": { "0100000000010000": "Super Mario Odyssey" },
  "accounts": { "00112233445566778899AABBCCDDEEFF": "Léa" } }
```

```json
{ "schema": 1, "ts": 1760000000, "days": ["2026-10-03", "2026-10-04", "2026-10-05", "2026-10-06", "2026-10-07", "2026-10-08", "2026-10-09"],
  "games": [ { "app_id": "0100000000010000", "min": [0, 45, 60, 30, 0, 90, 60], "total_min": 5400, "launches": 123 } ],
  "accounts": [ { "uid": "00112233445566778899AABBCCDDEEFF", "min": [0, 45, 60, 30, 0, 90, 60] } ] }
```

## Orders: `<entity>/set`

The publisher (HA, the integration, anything) publishes a **retained scalar**
payload (text, no JSON). The console applies it through the same guarded path
as a press on the console, publishes an `event`, **clears the retained
message** (an empty retained publish on the same topic) and republishes
`state`. An order the broker replays at reconnection (delivered with the RETAIN
flag) is applied only if it differs from the current state. Orders received
while PlayGuard is open are applied by PlayGuard, with its policy (*ask* shows
the usual confirmation on the console).

| Entity | Payload | Bounds | Needs `remote_timer_writes` | Effect |
|---|---|---|---|---|
| `limit_sun` … `limit_sat` | integer | 0–1440, 1440 = no limit | yes | That weekday's limit. Several within 2 s are merged into one write. |
| `limit_uniform` | integer | 0–1440 | yes | The same limit every day. |
| `limits_week` | `n,n,n,n,n,n,n` | Sunday first, each 0–1440 | yes | All seven days in one order. |
| `max_screentime_today` | integer | 0–1440 | yes | Today's weekday limit only. |
| `play_timer` | `ON` / `OFF` | | yes | `OFF` removes the limit (timer off); `ON` restores the last limits known to PlayGuard, or is rejected if none. |
| `console_lock` | `ON` / `OFF` | | yes | 0 minutes every day (`ON`), saving the limits; `OFF` puts them back. |
| `add_bonus_time` | integer | 5–180 | yes | Extra minutes today; put back at the next local day, as in PlayGuard. |
| `stop_today` | `PRESS` | | yes | 0 minutes today. |
| `locked` | `ON` / `OFF` | | yes | `OFF` unlocks parental controls temporarily with the stored PIN; `ON` locks again. |
| `lock_now` | `PRESS` | | no | Locks again (1007), always allowed. |
| `restriction_level` | `none` / `young_child` / `child` / `teen` / `custom` | | no | 1033. |
| `vr_mode` | `ON` / `OFF` | | no | `ON` = restricted (1063). |
| `sns_post_restriction`, `free_communication` | `ON` / `OFF` | custom level only | no | 1036; rejected at another level. |
| `play_timer_alarm` | `ON` / `OFF` | | no | `OFF` = the "time's up" alarm disabled (1953). |
| `play_timer_running` | `ON` / `OFF` | | yes | Resume / pause the countdown (1451 / 1452). |
| `bedtime_enabled` | `ON` / `OFF` | | yes | Bedtime alarm on or off. |
| `bedtime_alarm`, `bedtime_end_time` | `HH:MM` (`HH:MM:SS` accepted) | | yes | Bedtime and the "allowed again" time. |
| `profile` | a profile name | must exist on the SD card | yes | Applies the saved profile's limits. |
| `sync_now` | `PRESS` | | no | Republish everything now. |
| `export_report` | `PRESS` | | no | Save a diagnostic report on the SD card (and publish it if `publish_report`). |
| `sync_network_clock` | `PRESS` | | no | Measure the configured NTP server and set the network clock, as the Clock tab does. |
| `discovery` | `on` / `off` | | no | Native HA discovery. `off` clears the discovery payload; used by the HA integration when it takes over. |

There is **no** order that deletes parental controls, unlinks the companion
app, or reads, sets or changes the PIN.

Rejection reasons (`event.reason`): `unknown_entity`, `invalid`,
`out_of_range`, `policy_off` (policy is *off*), `timer_writes_disabled`,
`read_only`, `not_confirmed` (declined on the console under *ask*), `gated`
(the timer counts down and the unlock did not happen), `unlock_failed`,
`not_custom`, `no_such_profile`, `pctl_error` (with `rc`).

## `event`

```json
{ "schema": 1, "source": "app", "ts": 1760000000,
  "event_type": "command_rejected", "entity": "limit_mon", "payload": "5000",
  "reason": "out_of_range", "rc": null }
```

`event_type`: `command_applied`, `command_rejected`, `unlocked`, `relocked`,
`limit_reached`, `extra_restored`, `agent_started`, `agent_stopping`,
`report_saved`. Not retained: a consumer that was away misses them on
purpose; the state carries `link.last_result`.

## Home Assistant native discovery

One retained, device-based payload on
`homeassistant/device/playguard_<id>/config`: `dev` (identifiers
`playguard_<id>`, name, manufacturer `PlayGuard`, model `Nintendo Switch`,
`sw_version`), `o` (origin: `playguard`, version, support URL),
`availability_topic`, a default `state_topic` of `playguard/<id>/state`, and
`cmps` keyed by object id. Every component uses a `value_template` over the
shared state; writable ones have `command_topic` `playguard/<id>/<entity>/set`,
`retain: true` and `optimistic: true`. The entities that need
`remote_timer_writes` are not announced while it is off.

| Platform | Object ids |
|---|---|
| binary_sensor | `parental_controls_enabled`, `temp_unlocked`, `companion_linked`, `timer_enabled`, `limit_reached`, `pin_set`, `clock_accurate`, `alarm_disabled`, `console_locked`, `agent_running` |
| sensor | `used_screen_time`, `used_screen_time_log`, `screen_time_remaining`, `max_screentime_today` (read), `extended_screen_time`, `spent_raw_1952`, `now_playing`, `restriction_level`, `rating_age`, `firmware`, `atmosphere`, `local_date`, `last_sync`, `companion_last_sync`, `last_result`, `pending_orders` |
| number | `limit_sun` … `limit_sat`, `limit_uniform`, `max_screentime_today` (0–1440, step 5, `min`, `duration`) |
| time | `bedtime_alarm`, `bedtime_end_time` |
| select | `restriction_level`, `profile` |
| switch | `play_timer`, `console_lock`, `vr_mode`, `sns_post_restriction`, `free_communication`, `play_timer_alarm`, `play_timer_running`, `bedtime_enabled`, `locked` |
| button | `sync_now`, `add_bonus_time_15`, `add_bonus_time_30`, `add_bonus_time_60`, `stop_today`, `lock_now`, `export_report`, `sync_network_clock` |
| event | `events` |

`used_screen_time` is the timer's figure, `used_screen_time_log` the play
log's; both are `measurement`, in minutes, device class `duration`. Diagnostic
entities (`firmware`, `atmosphere`, `last_sync`, `spent_raw_1952`,
`local_date`) carry `entity_category: diagnostic`; numbers and switches
`config`. Ids and names follow HA's `nintendo_parental_controls` integration
where the meaning matches.

The HA integration, when installed, publishes `discovery/set = off`, and
creates its own entities with the same object ids.

## Files on the SD card

| File | Written by | Content |
|---|---|---|
| `switch/playguard/sync.conf` | PlayGuard (Sync screen); hand-editable | The link's settings, `key=value`, one per line (below). |
| `switch/playguard/sync/ca.pem` | the user | Optional CA certificate for a private broker. |
| `switch/playguard/sync/nro_state.txt` | PlayGuard, from `config::save()` | `relock_pending`, the extra-time record, the console lock and its saved limits, `fw_gate_choice`, `pin_lock`: what the agent must know to act on PlayGuard's behalf. |
| `switch/playguard/sync/agent_state.txt` | the agent | The same records, for what the agent did itself. PlayGuard adopts them at start-up. |
| `switch/playguard/sync/agent_events.log` | the agent | One event per line; PlayGuard imports them into `history.json` (source `remote`) and truncates the file. |
| `switch/playguard/sync/agent_status.txt` | the agent | Connected, last publish, last error, version: the Sync screen when the agent is not reachable over IPC. |
| `switch/playguard/sync/agent.log` | the agent | Rolling log, 64 KiB. |
| `atmosphere/contents/<tid>/{exefs.nsp, flags/boot2.flag, toolbox.json, version.txt}` | the Modules screen, or the user | The installed module. |

Each file has exactly one writer; the IPC service is the live channel, the
files the cold one. `sync.conf` is never included in a diagnostic report or
an online upload.

`sync.conf` keys: `schema=1`, `enabled`, `host`, `port` (1883; 8883 turns
`tls` on by default), `tls`, `ca_file`, `username`, `password`,
`allow_anonymous`, `console_id`, `console_name`, `policy` (`ask` / `auto` /
`off`; the agent alone only applies `auto`), `remote_timer_writes`,
`publish_report`, `publish_activity`, `ha_discovery`, `poll_s` (30, 10–300),
`log_level`. Unknown keys are kept.

## IPC service `pg:agent` (PlayGuard ↔ agent)

Hosted by the agent; PlayGuard connects at start-up. **An open session means
PlayGuard is running**: the agent stops reading pctl while PlayGuard is in the
foreground, and resumes when the session closes (exit or crash). Commands
(ids are provisional until phase B):

| Id | Command | In | Out | Effect |
|---|---|---|---|---|
| 0 | `Hello` | app version, protocol version | agent version, protocol version, console id, connected | Handshake. A protocol mismatch makes PlayGuard offer an update. |
| 1 | `SetForeground` | bool | | In the foreground PlayGuard reads pctl and pushes; in the background the agent reads. |
| 2 | `PushState` | JSON (buffer) | | Publish `state` now. PlayGuard calls it after every write and at the Overview's rhythm. |
| 3 | `PushActivity` | JSON | | Publish `activity` now. |
| 4 | `PushNames` | JSON | | Publish `names`. |
| 5 | `PushWeek` | JSON | | Publish `week`. |
| 6 | `GetOrderEvent` | | event handle | Signalled when an order waits. |
| 7 | `PopOrder` | | id, entity, payload, retained flag | The next order PlayGuard must apply (none → empty). |
| 8 | `OrderResult` | id, rc, applied, reason | | The agent publishes the event, clears the retained order, republishes the state. |
| 9 | `SyncNow` | | | Republish availability, discovery, state, activity, names. |
| 10 | `GetStatus` | | connected, broker, last publish, last error, pending, uptime, version | The Sync screen's live status. |
| 11 | `GetEvents` | | ring of the last events | Imported into the change history at start-up. |
| 12 | `GetRecords` | | extra-time / console-lock / relock records | Adopted into `config` at start-up. |
| 13 | `ReloadConfig` | | | Re-read `sync.conf`. |
| 14 | `GetLog` | | last 200 lines | The Sync screen's log view. |
| 15 | `PrepareShutdown` | | | Publish `offline`, disconnect: first step of an update or a stop from PlayGuard. |

Not available on the desktop build; the engine's state machine behind it is
host-tested with a fake transport.

## Schema and compatibility

- `schema` is on every JSON document and in `sync.conf`. Adding a field does
  not bump it; removing or renaming one, or changing an order's payload, does.
- The HA integration pins the schema it understands and raises a repair issue
  on a newer one.
- The entity table (`source/sync/sync_entities.h`) is exported by a host test
  as `entities.json`; the integration carries a copy and its tests fail when
  the two diverge.
