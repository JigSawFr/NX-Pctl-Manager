# NX-Pctl-Manager

[![build](https://github.com/JigSawFr/NX-Pctl-Manager/actions/workflows/build.yml/badge.svg)](https://github.com/JigSawFr/NX-Pctl-Manager/actions/workflows/build.yml)
[![license: GPLv3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![latest release](https://img.shields.io/github/v/release/JigSawFr/NX-Pctl-Manager)](https://github.com/JigSawFr/NX-Pctl-Manager/releases/latest)

*[Lire en français](README.fr.md)*

A Nintendo Switch parental-controls manager — **no phone app, no Nintendo account, no internet needed**. Set the daily play-time limit directly on the console, change restrictions, set the network clock, and reset / delete the PIN or unlink the companion app.

![Overview](images/screenshots/dashboard.png)

> ⚠️ **Requires custom firmware (Atmosphère).** The app talks to the restricted system `pctl` service, so it only runs on a hacked console. It does not bypass any account or online check: it brings the settings that are otherwise only in the phone app (or deep in System Settings) onto the console, for offline use. Some commands it uses are `*ForDebug` ones. **Use at your own risk.**

## Compatibility

| | Supported | Notes |
|---|---|---|
| Firmware | **21.0.0 → 23.0.1** | The play-time limit layout (0x44 bytes) exists since 21.0.0. Below that, every tab works except the play timer. A newer firmware shows a one-time warning: reading is safe, check the result of changes. |
| Atmosphère | **1.11.x → 1.12.0** | 1.12.0 adds 23.0.0 support. The app shows the detected Atmosphère version. |
| Launchers | hbmenu, **sphaira**, **Homebrew App Store** | Launching over a game (title override) is recommended; the app shows whether it runs as an application or as an applet (album). |
| Hardware-tested | 22.1.0 / Atmosphère 1.11.1 | 23.0.1 / 1.12.0 is supported by the command table (switchbrew) but not yet tested on hardware: reports are welcome. |

**Why 4.0.0 fixes the 22.5 crashes.** `pctl:a`, the privileged parental-control service, accepts a **single session**. Earlier versions kept it open the whole time, so the HOME-menu PIN prompt (or the PIN applet) could not get it and Atmosphère could crash. Since 4.0.0 every action opens the session, does its work and releases it immediately; periodic refreshes pause while the app is in the background. *(Diagnosis by [anbingxi's fork](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

## Features

The app is organised in tabs, like System Settings.

| Tab | What you can do |
|---|---|
| **Overview** | Parental-control state, PIN, restriction level, a gauge of today's play time, bedtime alarm, network-clock accuracy, companion-app link, firmware / Atmosphère / compatibility. Refreshes every 5 s (X refreshes now). |
| **Play timer** | Same limit every day (quick list or any value), a different limit per day (with Monday–Friday / weekend presets, "no limit" per day), remove the limit, **profiles** saved on the SD card (e.g. *School week*, *Holidays*), bedtime alarm (read-only). Advanced, opt-in: "time's up" alarm on/off, pause / resume the countdown. |
| **Restrictions** | Restriction level (None, Young child, Child, Teen, Custom); in Custom: age rating, social-media posting, communication with others; VR mode; rating organisation. |
| **Network clock** | Console / network clocks, time zone, accuracy. Pick a public NTP server (≈ 50 built-in, by region, or your own), **measure** against 3 servers (median, warning when they disagree) and **set the network clock**. The play timer relies on this clock; a console that never reaches Nintendo's servers keeps it inaccurate. |
| **Companion app** | Whether the Nintendo Switch Parental Controls phone app is linked, last synchronisation, unlink it (otherwise its next sync overwrites the limits set here). |
| **PIN & security** | Set / change the PIN (system PIN screen), unlock temporarily, **lock again now**, delete all parental controls (double confirmation, irreversible). |
| **Tools & about** | Export a diagnostic report, language (system / English / Français), theme (system / light / dark), advanced actions, versions. |

![Play timer](images/screenshots/play_timer.png)
![Per-day limits](images/screenshots/per_day.png)

### How the play-time limit is written safely

If the timer is counting down, overwriting its configuration destabilises Atmosphère. So before any write the app checks the state; if the timer is active it asks, unlocks parental controls temporarily (with the stored PIN — **you don't need to remember it**), checks that the system really reports the unlock, writes, then offers to **lock again right away**. The service layer re-checks the same state just before writing, so no screen can skip that safeguard.

`0` minutes means *no play that day*; *Remove the play-time limit* turns the timer off. ⚠️ Don't set a limit below the time already played today: as soon as parental controls are locked again, the game is suspended.

## Install

Pick one:

- **Homebrew App Store** or **sphaira's App Store** (same catalogue): search for *Pctl Manager* once the listing is approved. New GitHub releases are picked up automatically.
- **sphaira's GitHub menu**: the release zip already contains the entry (`/config/sphaira/github/nx_pctl_manager.json`), so after a first install you can update from *GitHub* in sphaira.
- **Manually**: download `nx_pctl_manager.zip` from the [Releases](../../releases/latest) and extract it to the **root** of the SD card. The app lands in `sd:/switch/nx_pctl_manager/`.

> Upgrading from 3.x? Delete the old `sd:/switch/nx_pctl_manager.nro`, otherwise hbmenu shows the app twice.

Files the app writes: `sd:/switch/nx_pctl_manager/config.json` (preferences), `profiles/` (saved limits), `logs/` (diagnostics). More in [packaging/README.md](packaging/README.md).

### First steps

1. **Parental controls not set up yet:** System Settings › Parental Controls › set a PIN (skip the phone-app pairing). Then open the app.
2. **Already paired with the phone app:** open *Companion app* › *Unlink*, otherwise the next sync overwrites what you set here.
3. *Play timer* › *Same limit every day* (or *A different limit for each day…*).
4. If the network clock is not accurate (Overview), use *Network clock* › *Measure* then *Set the network clock* (enable *Synchronise Clock via Internet* in System Settings first).

Controls: ↑/↓ move, Ⓐ confirm, Ⓑ back (on the sidebar: exit), Ⓧ refresh.

## Bug reports

*Tools & about* › *Export a diagnostic report* saves a text file in `sd:/switch/nx_pctl_manager/logs/` with the firmware, Atmosphère version, clocks and the raw result of every parental-control query. **It never contains the PIN.** Attach it to the issue.

A **read-only "Pctl Diagnostics" build** (`READ_ONLY=1`) cannot change anything; it is useful to investigate a new firmware safely.

## Build from source

```sh
make test        # unit tests of the C service layer (any gcc, no devkitPro)
make desktop     # the UI on Linux with a simulated console (needs GLFW / X11 / D-Bus dev packages)
make             # ./nx_pctl_manager.nro      (devkitPro switch-dev, DEVKITPRO set)
make dist        # ./nx_pctl_manager.zip      (SD-card layout)
make PROBE=1     # + diagnostic shortcuts in the Play timer tab
make READ_ONLY=1 # "Pctl Diagnostics" build
./run.sh [ip]    # build in the devkitpro/devkita64 Docker image, optionally nxlink to a console
```

The desktop build runs the real borealis UI against `source/sim/` (environment knobs: `NXPM_SIM_FW=20.5.0`, `NXPM_SIM_NO_CFW=1`, `NXPM_SIM_TIMER_OFF=1`). `tools/desktop_smoke.py` clicks through every screen headlessly; CI runs it with the unit tests, the resource checks (`tools/check_resources.py`) and the three Switch builds.

Layout: `source/core/` (C, libnx: `pctl_ops`, `time_ops`, `sysinfo`), `source/tab/` (one class per tab), `source/ui/` (dialogs, formatting), `source/util/` (NTP, config, profiles, diagnostics), `resources/` (XML layouts, `i18n/en-US`, `i18n/fr`).

## License

GPLv3 (see [`LICENSE`](LICENSE)).

### Third-party and credits

- UI: **[borealis](https://github.com/xfangfang/borealis)** (Apache 2.0), pinned at `extern/borealis/`.
- fw 22.5 diagnosis, session release and NTP synchronisation adapted from **[anbingxi/NX-Pctl-Manager](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly)**.
- Command reference: [switchbrew — Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).
