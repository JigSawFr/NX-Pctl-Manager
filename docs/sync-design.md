# Remote link design: the optional MQTT agent and Home Assistant

**Status: phases A, B0 and B implemented** (the C core in `source/sync/`,
PlayGuard's autonomous mode and its *Remote access* screen, CI against a
local Mosquitto; the optional modules screen; the agent sysmodule, its
`pg:agent` service and PlayGuard's agent mode, with a simulated agent in CI;
user guide in [`home-assistant.md`](home-assistant.md)). Not yet tried on a
console: the agent itself (its IPC server, TLS, its start without a reboot).
Phase C (the Home Assistant integration) is not done yet. This document records the decisions
behind an optional link between PlayGuard and a home-automation setup, so that
a parent can see the console's state and play time, and change the limits,
from a phone, through [Home Assistant](https://www.home-assistant.io/) (HA).
The wire contract (topics, payloads, files, IPC) is in
[`sync-protocol.md`](sync-protocol.md). This is the design behind the
[roadmap's horizon](../ROADMAP.md#horizon); the orders it carries are the
companion-app features listed in [`companion-app.md`](companion-app.md). Both
documents are meant to be edited as the implementation lands; a fact that
turns out wrong on hardware should be fixed here first.

Nothing here changes what PlayGuard does **by default**: without the link
switched on and the agent installed, PlayGuard stays offline and nothing runs
in the background, as today.

## Goals and non-goals

Goals:

- From a phone: see the parental-control state, today's play time and the
  limits; change the limits, the restrictions and the bedtime; get notified
  when the time is up or an order could not be applied.
- The console is often asleep or off, so **pending orders and collected data
  must live off-console**. The console pulls the latest orders and pushes its
  state when it can.
- **Realtime while a game runs**, not only while PlayGuard is open: an order
  sent from the phone is applied within a minute.
- Optional, installable from PlayGuard itself, removable without a trace.
- Long-term play statistics (months), per day and per game.

Non-goals:

- A native mobile app, or a web app of our own to host.
- A server of our own ("hub"): HA already stores, graphs, automates and notifies.
- Remote deletion of parental controls, unlinking of the companion app, or
  anything touching the PIN. These stay on the console, on purpose.

## What the code base imposes

- **Play history already exists on the console.** `source/core/playstats.c`
  reads the system's play log (`pdm`), which covers time played while PlayGuard
  was closed. PlayGuard only folds the last 7 local days; the long-term
  archive has to live elsewhere.
- **Two "today" counters.** The console timer's own (today's limit minus 1454
  `GetPlayTimerRemainingTime`) and the play log's. They differ (see
  [`parental-controls.md`](parental-controls.md)); both are published.
- **Every write is guarded.** `core_change_allowed()` enforces runtime
  read-only mode and the *ask for the PIN* hook (`core_set_change_check`),
  and `pt_write_gate` refuses a play-timer write while the timer counts down
  unless parental controls are temporarily unlocked. Remote orders go through
  exactly the same path as a press on the console.
- **Cross-run state lives in `config.json`**: extra time granted today,
  the console lock's saved limits, `relock_pending`. A background agent that
  writes the timer has to know about them.
- **`util/http.cpp` is HTTPS-only and serialises requests.** HA is usually
  reached over plain `http://…:8123`, and a LAN broker has no public
  certificate: HTTP is not the transport for this.
- **`config.json` is uploaded with every online report.** Credentials cannot
  live in it (`github_token` is already a separate file for that reason).
- **All pctl calls run on the UI thread**; nothing in the app runs in the
  background and no thread is joined at exit. The link adds the first
  long-lived thread, and must never call pctl from it.
- **The recovery sysmodule** (`sysmodule/rescue/`) is the template for building,
  packaging and shipping a boot sysmodule; it is one-shot and has no network.
- **Positioning.** The README says "no internet" and "nothing runs in the
  background", and sells the latter against replacement sysmodules. When the
  link ships, that becomes "nothing by default; two optional modules".

## Decisions

### Transport: MQTT, broker-agnostic, Mosquitto by default

**Decision.** The console speaks plain MQTT 3.1.1 (retained messages, last
will, QoS 1 inbound) to any broker. The documentation sets up Mosquitto, HA's
add-on, because that is what HA users have; nothing depends on a broker's
own features (no `$SYS`, no MQTT 5, no dynamic security). TLS is optional per
broker, through the console's own `ssl` service (so no TLS library of ours),
with an optional CA file for private brokers.

**Why.** The broker *is* the off-console memory: a retained message on a
command topic is a pending order that survives the console being off; the
last will turns into an honest online/offline flag; HA's MQTT discovery makes
the console appear in HA with no code on the HA side. The client is small
enough for a sysmodule. Raw TCP on the LAN sidesteps the HTTPS-only wrapper
and its one-request-at-a-time mutex.

**Rejected.** A REST API or webhooks into HA (needs HTTPS to HA, no cache of
pending orders, libcurl too heavy for the agent). The console as an HTTP server
polled by HA (the console sleeps). A hub of our own (one more code base to
host, for things HA already does).

### A background agent, both processes active, one broker client

**Decision.** `playguard-agent`, an optional Atmosphère boot sysmodule, is
the **only MQTT client** when it runs. PlayGuard, when open, talks to it over
a small IPC service (`pg:agent`): it pushes its own fresh snapshots after every
change, receives the orders that arrive while it is open and applies them with
its own policy and dialogs, and shows the agent's live status. Without the
agent, PlayGuard runs the same engine itself with the broker transport, while
it is open.

**Why.** One source of truth for HA, no online/offline flicker when PlayGuard
opens, nothing stale on either side: a limit changed on the console is in HA
within two seconds, an order from HA while PlayGuard is open shows up in
PlayGuard. Who reads `pctl` is unambiguous: PlayGuard in the foreground,
the agent otherwise (PlayGuard's refreshes already pause out of focus; the
developer-mode play-timer recorder, the one exception, will pause too), with
the hand-over on an IPC call and the agent's autonomy restored when the
session drops (exit or crash).

**Rejected.** Switching the agent off while PlayGuard runs (hand-over through
files on the SD card, data stale at the boundaries, HA sees the console go
away at every launch). Routing all of PlayGuard's pctl calls through the agent
(a rewrite of the UI, the PIN applet cannot be driven from a sysmodule, and
nothing would be testable off hardware).

### Home Assistant: native discovery first, a HACS integration on top

**Decision.** The console publishes HA's native MQTT discovery (one
device-based payload), so HA shows the console with no install. A custom
integration, in its own repository (`playguard-homeassistant`, HACS), is the
enrichment layer: it detects consoles on the broker by itself (HA's
MQTT-triggered discovery flow), takes over from the native discovery to avoid
duplicate entities, adds game names, per-game and per-account sensors, imports
the daily totals as long-term statistics (months of history with no database
of ours), and provides services, device triggers, repairs and diagnostics.

**Why.** Two levels: zero-install for the curious, a proper integration for
the rest; HACS wants one integration per repository with its own releases.

**Vocabulary.** HA ships a core `nintendo_parental_controls` integration
(Nintendo's cloud API) since 2025.11. Our entity names follow it where the
meaning is the same (`used_screen_time`, `screen_time_remaining`,
`max_screentime_today`, `bedtime_alarm`, `bedtime_end_time`, `add_bonus_time`),
so a parent moving from the phone app to PlayGuard finds the same words.

### One C core, two hosts, no vendored MQTT

**Decision.** `source/sync/` is plain C11 with no heap allocation and
caller-provided buffers: the MQTT codec and client, the state snapshot, the
discovery payload, the order parser, the gated-write sequence, the flat config
parser and the engine's state machine, behind two small vtables (transport,
console). The app links it with a C++ shell on the UI side; the agent links it
with libnx directly, alongside `source/core/pctl_ops.c` (the recovery sysmodule
duplicated those calls; the agent will not).

**Why.** The desktop build is the only automated test bench: with the app
hosting the engine, the whole protocol runs in CI against a local Mosquitto.
The agent adds only its transport and its process shell, which are hardware
only. CodeQL ignores `extern/` and the host tests build with `-Werror` and
the sanitizers; a hand-written client (NTP, zip, SHA-256, XLSX and PDF already
are) stays under both.

### Secrets and defaults

- Everything about the link, broker password included, lives in one flat
  file, `sd:/switch/playguard/sync.conf`, written by PlayGuard's Sync screen
  and read by both processes. Nothing goes into `config.json`.
- `remote_timer_writes` is **off by default**: with it off, the link is
  monitoring plus *Lock now* plus the restrictions; with it on, a remote order
  may unlock parental controls for a moment with the stored PIN, write, and
  lock again, exactly as a press on the console would. The gate refuses any
  timer write on a console that has an active limit otherwise, so the setting
  is worded for what it really does.
- An anonymous broker is refused unless `allow_anonymous` is set. The
  documentation sets up a dedicated broker user with an ACL on
  `playguard/<id>/#`. Whoever can publish on that topic can lock the console
  or, with `remote_timer_writes`, change the limits: the broker's access
  control is the fence, and `SECURITY.md` says so.
- Never remote: 1043 `DeleteSettings`, 1941 `DeletePairing`, the PIN. The
  order parser has no intent for them; a host test proves it.

## Architecture

```
 phone ── Home Assistant ── MQTT broker (Mosquitto) ──┐
          (native discovery, or the                   │ TCP, optional TLS
           playguard HACS integration)                │
                                                      ▼
                                        playguard-agent (boot sysmodule)
                                        the only MQTT client
                                            ▲ IPC pg:agent
                                            │ (session open = PlayGuard open)
                                        PlayGuard (NRO)
                                        reads pctl in the foreground,
                                        pushes snapshots, applies forwarded orders
```

Who does what, by situation:

| PlayGuard | pctl is read by | Orders are applied by | Snapshots published by |
|---|---|---|---|
| closed | the agent, every 30 s | the agent (policy *auto* only, no dialog) | the agent |
| open, in focus | PlayGuard (its own refreshes) | PlayGuard, with its policy (*ask* shows the usual confirmation) | the agent, from what PlayGuard pushes |
| open, out of focus (HOME menu over it) | the agent | PlayGuard, when back in focus | the agent |
| open, agent not installed | PlayGuard | PlayGuard | PlayGuard, with its own broker client |

The life of an order: HA publishes a retained scalar on
`playguard/<id>/<entity>/set` → whoever owns pctl parses it (`sync_apply`),
applies it through the ordinary guarded path (`sync_exec` mirrors
`pt_flow`: unlock if needed, write, lock again, also on failure) → an
`/event` says applied or rejected and why → the retained order is **cleared**
(empty retained publish) so it is used once → the state is republished. An
order replayed by the broker at reconnection (RETAIN flag set) is applied only
if it differs from the current state. HA's writable entities are
`optimistic`, so a slider moved while the console sleeps keeps its value; the
`pending_orders` sensor and the events say what is still waiting.

Play statistics: the agent publishes today's totals from the play log on each
cycle and, at the first cycle after local midnight, yesterday's final totals on
`activity/<date>` (the last 14 days stay retained). PlayGuard, when open,
publishes the 7-day table and the table of game names (it has the NACP data
cached; the agent never loads it). The HA integration turns the daily totals
into long-term statistics.

### The agent

- Built like the recovery sysmodule (`sysmodule/agent/`, shared
  `sysmodule/common.mk`), `__nx_applet_type = None`, a static heap of
  512–768 KiB, a minimal `socketInitialize` (two 16 KiB TCP buffers, no UDP:
  about 32 KiB of transfer memory). Services: `pctl:a/s/r`, `fsp-srv`,
  `set:sys`, `time:u`, `nifm:u`, `bsd:u`, `sfdnsres`, `ssl`, `pdm:qry`,
  `pm:dmnt`; hosts `pg:agent` (which needs the session syscalls 0x40–0x45, as
  sys-clk's `perms.json` shows).
- Starts after qlaunch and once `nifm` reports a connection; reads
  `sync.conf`; connects; then a 30 s cycle (5 s for a minute after an
  order): skip while PlayGuard holds the IPC session in the foreground, skip
  while the PIN applet process exists, back off for a minute when
  `pctlInitialize` fails; otherwise one snapshot (every pctl call opens and
  closes its own session, as in the app), today's play-log fold, publish,
  handle inbound orders, and at midnight publish yesterday and restore the
  extra time if its record still matches.
- Two threads: the MQTT loop in 500 ms slices, and the IPC server blocked on
  `svcReplyAndReceive`, joined by a mailbox under a mutex.
- Sleep: the last will marks the console offline within 45 s; the socket error
  or the `nifm` status at wake triggers a reconnection with back-off. No
  `psc:m` registration (not confirmed safe for a third-party module).
- `pctl:a` contention: none between the two processes (one reader at a time);
  with the HOME menu's PIN prompt, a few milliseconds every 30 s, cycles
  skipped while the PIN applet runs, and libnx's fallback to `pctl:s` turns a
  collision into one degraded call rather than a crash. The residual risk is
  stated in the agent's README and is the first thing to try on hardware.

### Installing, configuring and updating the modules from PlayGuard

- A *Modules* screen lists the recovery module and the agent: installed,
  version, enabled at boot (`boot2.flag`), running (`pmdmntGetProcessId`),
  with install / update, enable / disable, start / stop now
  (`pmshellLaunchProgram` / `pmshellTerminateProgram`, reachable from a
  homebrew launched by hbloader: its npdm grants every service) and uninstall.
  Manual installation from the zips stays possible.
- The module binaries are **bundled in PlayGuard's romfs**, built from the
  same commit (about 0.5 MiB more); updating PlayGuard makes the new agent
  available, and PlayGuard offers to deploy it: tell the agent to go offline
  cleanly, terminate it, swap `exefs.nsp` with a backup kept, relaunch, wait for
  a consistent `Hello`, and roll back if that fails. The protocol version in
  `Hello` is what tells PlayGuard an installed agent is too old.
- All of the agent's settings are in `sync.conf`, edited from the Sync
  screen, with a `ReloadConfig` call afterwards; the screen also shows the
  agent's live status and log, and has *Sync now*.

### The HA integration (separate repository)

`manifest.json` declares `"mqtt": ["playguard/+/state"]`, so HA starts the
integration's discovery flow as soon as a console publishes (a retained
`/state` re-triggers it at every HA start; the flow aborts on an already
configured `console_id`). It then publishes `discovery/set = off` so the
console clears its native discovery payload, and creates its own entities with
the same object ids. Daily totals go through
`recorder.statistics.async_add_external_statistics` (`playguard:<id>_…`,
one point per day). Services (`set_limits`, `apply_profile`, `add_bonus_time`,
`stop_today`, `lock_now`, `sync_now`, `request_report`) publish the same
scalar orders with the same bounds; the entity table is exported from this
repository's tests as `entities.json` and a test on the integration side fails
when the two diverge. The last diagnostic report is kept and downloadable from
the device's diagnostics.

## Phasing

| Phase | Content | What it gives |
|---|---|---|
| A | `source/sync/` with host tests; PlayGuard hosting the engine itself (no agent); native HA discovery; Mosquitto in the desktop smoke test; this protocol | State and statistics in HA whenever PlayGuard is open; the protocol proven in CI |
| B0 | The *Modules* screen, deploying the recovery module | The recovery module without a computer |
| B | The agent, the `pg:agent` service, PlayGuard in agent mode, bundling, release assets, update from the app | Realtime; nothing stale between PlayGuard and HA |
| C | The HACS integration (its own repository, as soon as the topics are frozen) | Game names, per game and per account, long-term statistics, automations, reports in HA |

Verification, per phase, is listed with the files in the implementation
notes; the hardware checks that matter most are the `pctl:a` contention with
the HOME menu's PIN prompt while the agent runs, sleep and wake, the agent
update with roll-back, and an order sent from HA while a game runs.

## Verified and open

| Fact | Status |
|---|---|
| A homebrew launched by hbloader can use `pm:shell` and `pm:dmnt` (`hbl.json` grants `*`); Hekate-Toolbox, TriPlayer and SwitchPresence do | verified in sources |
| A sysmodule with sockets: ~32 KiB of transfer memory with a minimal config, 512–768 KiB of heap; `service_host` needs syscalls 0x40–0x45 | verified in libnx and sys-clk / sys-ftpd sources |
| TLS through the console's `ssl` service from a socket, with the system CA store and an importable CA | verified in libnx (`ssl.h`, `socketSslConnectionSetSocketDescriptor`), used by nx.js |
| HA MQTT discovery: `optimistic`, `retain`, device-based payloads, the `event` and `time` platforms, removal by an empty retained payload | verified in HA documentation |
| HA MQTT-triggered discovery flows for custom integrations; `async_add_external_statistics`; HACS: one integration per repository | verified in HA core and HACS documentation |
| `psc:m` registration from a third-party sysmodule | open, kept out of scope |
| The PIN applet's process id (`0x0100000000001001`, LibraryAppletAuth), used to skip agent cycles | to check on switchbrew |
| When the console's own "time spent" resets (midnight or the "allowed again" time) | open, see `parental-controls.md`; the daily totals use the play log, not the timer |
| `pctl:a` contention between the agent and the HOME menu's PIN prompt | hardware test, first thing of phase B |
