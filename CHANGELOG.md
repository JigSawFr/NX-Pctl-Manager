# Changelog

## v4.0.0

Compatibility with firmware **23.0.1 / Atmosphère 1.12.0**, a fix for the Atmosphère crashes reported on 22.5, a new tabbed interface and new features.

**Compatibility and stability**
- Every action opens the privileged `pctl:a` session and releases it right away (it accepts a single session; keeping it open broke the HOME-menu PIN prompt on 22.5). Refreshes pause while the app is in the background.
- Firmware and Atmosphère detection. The play timer requires 21.0.0+ (0x44-byte settings); older firmware keeps every other tab. Firmware newer than 23.0.1 shows a one-time warning.
- The play-timer write gate now lives in the service layer and also covers *Remove the limit*. The temporary unlock is verified (`IsRestrictionTemporaryUnlocked`) before writing.
- `READ_ONLY=1` builds a "Pctl Diagnostics" app that cannot change anything.

**Interface**
- Tabs: Overview (with a play-time gauge), Play timer, Restrictions, Network clock, Companion app, PIN & security, Tools & about.
- French translation; language and theme preferences (follows the console by default).
- Readable error messages next to every error code.

**New features**
- Per-day editor: "no limit" per day, Monday–Friday / weekend presets, copy a day, discard changes.
- Limit profiles saved on the SD card.
- Lock again right after a write (`RevertRestrictionTemporaryUnlocked`).
- Restrictions: level, age rating, social-media posting, communication, VR mode.
- Network clock: about 50 public NTP servers by region (or your own), measurement against 3 servers, then set the clock (adapted from anbingxi's fork).
- Bedtime alarm display; opt-in advanced actions ("time's up" alarm, pause / resume).
- Diagnostic report in `sd:/switch/nx_pctl_manager/logs/` (never contains the PIN).

**Distribution and development**
- The app now installs to `sd:/switch/nx_pctl_manager/` (delete the old `sd:/switch/nx_pctl_manager.nro`). The zip includes the sphaira GitHub entry; releases ship the raw `.nro` too (sphaira) and the `.zip` (Homebrew App Store).
- Unit tests of the C service layer, a desktop build of the UI with a simulated console, a headless smoke test, resource checks, and CI that verifies the tag matches the app version.

## v3.0.0

UI rewrite onto [borealis](https://github.com/xfangfang/borealis) — Horizon-system-style graphical UI replaces the v2 text console. **No functional changes** — every action behaves the same.

- App display name is now **"Pctl Manager"** (repo name `NX-Pctl-Manager` unchanged).
- Non-CFW boot shows a dedicated error screen (was a silent fall-through in v2).
- Build switched to CMake; `make` / `./run.sh` interfaces unchanged.
- `.nro` grew 265 KB → ~8.2 MB (borealis library + assets).

Tested on fw 22.1.0 / Atmosphère 1.11.1. `make PROBE=1` still adds the diagnostic dump cell.

## v2.0.0

First public release. CFW (Atmosphère) only; tested on fw 22.1.0 / Atmosphère 1.11.1.

- **Configure the daily play-time limit offline** — one for all days, per-day limits, or off. When the timer is active, writes turn parental controls off temporarily (the app reads the PIN automatically) so the write doesn't destabilise Atmosphère.
- Set / change PIN via the system passcode applet.
- Delete all parental controls — also the recovery path when the PIN is forgotten.
- Unlink the Nintendo Switch Parental Controls companion phone app.
- View status: safety level, PIN length, restrictions, play-timer state.

`make PROBE=1` adds a read-only "Dump current config" diagnostic cell (off in release builds).
