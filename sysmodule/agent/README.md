# playguard-agent — the remote link in the background

An optional Atmosphère sysmodule that keeps PlayGuard's remote link (MQTT,
Home Assistant: [`docs/home-assistant.md`](../../docs/home-assistant.md)) up
while PlayGuard is closed. Without it the link only runs while PlayGuard is
open; with it, the console stays online, publishes its state and play time,
and carries out the orders Home Assistant sends, whatever is running.

It is **optional**: PlayGuard carries a copy and installs it from *Tools ›
Optional modules*, which also starts it, stops it, updates it and removes it;
it is also its own download (`playguard-agent.zip`, to extract at the SD-card
root). It does nothing until *Preferences › Remote access* is set up.

## What it does

- Connects to the broker set in `sd:/switch/playguard/sync.conf` (the same
  settings as PlayGuard, same session engine: `source/sync/sync_engine.c`),
  as the client `pg-<id>-agent`, with `offline` as its last will.
- Every `poll_s` seconds, reads the parental controls and the play timer
  through PlayGuard's own service layer (`source/core/`), and the play log of
  the last 7 days (`pdm:qry`, no game names: those come from `names.txt`,
  which PlayGuard writes), and publishes `state`, `activity` and, after
  midnight, the finished day.
- Carries out orders **only under the *auto* policy**, through the same gate,
  unlock and relock as PlayGuard (`source/sync/sync_exec.c`). Under *ask*,
  orders wait on the broker until PlayGuard opens and asks on the console.
- Puts the usual limit back the day after extra time or "no more play today",
  as PlayGuard does when it opens.
- Changes nothing when PlayGuard was in read-only mode, or when the firmware
  is not the one PlayGuard last ran on (a system update PlayGuard has not
  checked): `sync/nro_state.txt`.

## With PlayGuard open

The agent stays the only MQTT client, so Home Assistant never sees the
console go offline. PlayGuard holds a session on the agent's IPC service
`pg:agent` ([`agent_ipc.h`](../../source/sync/agent_ipc.h)): in the
foreground it is the only one reading the parental controls and pushes what it
reads; orders go to PlayGuard, which applies its policy, its PIN check and its
change history. When PlayGuard closes or goes to the background, the agent
reads the console again by itself.

## Files

In `sd:/switch/playguard/`: it reads `sync.conf`, `sync/nro_state.txt`,
`sync/profiles.txt` and `sync/names.txt`; it writes `sync/agent_state.txt`
(what it changed: extra time, console lock, a relock pending),
`sync/agent_events.log` (the orders it carried out, imported into PlayGuard's
history) and `logs/agent_<time>.txt` (a report asked for from Home Assistant).
Details: [`docs/sync-protocol.md`](../../docs/sync-protocol.md).

## Build

```sh
make agent                 # -> sysmodule/agent/out/playguard-agent.nsp (needs devkitPro, DEVKITPRO set)
make dist-agent            # -> playguard-agent.zip  (SD-card layout)
make                       # the .nro, with this module bundled in its romfs
```

It links PlayGuard's C service layer and the remote link's core unchanged
(`source/core/`, `source/sync/`); its own code is `source/main.c` (the loop),
`source/agent_core.c` (what the loop and the IPC thread share, host-tested in
`tests/agent/`) and `source/agent_ipc.c` (the HIPC server).

Program ID `0x4200000000504741`, in the private range homebrew sysmodules use.
Static heap 1.5 MiB; one TCP socket; TLS through the console's `ssl` service.

## Not tested on a console yet

Everything here builds and its logic is host-tested, but the IPC server, the
start without a reboot from PlayGuard (`pm:shell`), TLS and the share of the
`pctl:a` service with the HOME menu's PIN prompt still have to be tried on
hardware.

## License

GPLv3 (see [`../../LICENSE`](../../LICENSE)).
