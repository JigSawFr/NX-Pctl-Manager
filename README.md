# PlayGuard

![PlayGuard](images/store/banner.png)

[![build](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml/badge.svg)](https://github.com/JigSawFr/PlayGuard/actions/workflows/build.yml)
[![license: GPLv3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![latest release](https://img.shields.io/github/v/release/JigSawFr/PlayGuard)](https://github.com/JigSawFr/PlayGuard/releases/latest)

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

**Why there are no 22.5 crashes.** `pctl:a`, the privileged parental-control service, accepts a **single session**. Older builds of the original app kept it open the whole time, so the HOME-menu PIN prompt (or the PIN applet) could not get it and Atmosphère could crash. PlayGuard opens the session for each action, does its work and releases it immediately; periodic refreshes pause while the app is in the background. *(Diagnosis by [anbingxi's fork](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

## Features

The app is organised in tabs, like System Settings.

| Tab | What you can do |
|---|---|
| **Overview** | Today first (a gauge of today's play time, today's limit, time left, bedtime alarm), then the parental-control state, PIN and restriction level, then what needs attention: network-clock accuracy, companion-app link (in amber while linked), firmware / compatibility only when there is a problem. Ⓐ on a line opens the matching tab; Ⓐ on *Today's limit* changes it right there. A banner with **Lock now** appears while parental controls are temporarily unlocked. Refreshes every 5 s (Ⓧ refreshes now). |
| **Play timer** | Same limit every day (quick list or any value), a different limit per day (quick values, Monday–Friday / weekend presets, "no limit" per day, a count of unsaved changes), remove the limit, **profiles** saved on the SD card (e.g. *School week*, *Holidays*), bedtime alarm (read-only). Advanced, opt-in: "time's up" alarm on/off, pause / resume the countdown. |
| **Restrictions** | Restriction level (None, Young child, Child, Teen, Custom); in Custom: age rating, social-media posting, communication with others; VR mode; rating organisation. |
| **Network clock** | Console / network clocks, time zone, accuracy. Pick a public NTP server (≈ 50 built-in, by region, or your own), **measure** against 3 servers (median, warning when they disagree) and **set the network clock**. The play timer relies on this clock; a console that never reaches Nintendo's servers keeps it inaccurate. |
| **Companion app** | Whether the Nintendo Switch Parental Controls phone app is linked, last synchronisation, unlink it (otherwise its next sync overwrites the limits set here). |
| **PIN & security** | Set / change the PIN (system PIN screen), unlock temporarily, **lock now**, delete all parental controls (two confirmations with different red buttons, irreversible). |
| **Tools & about** | Export a diagnostic report, language and theme (with an offer to restart), **lock again automatically after a change** (on by default), advanced actions. *Console*: firmware, Atmosphère, compatibility, **storage** (emuMMC or sysMMC), whether Atmosphère **blanks the serial number** (with the number the system sees, partly hidden until Ⓐ; a warning on emuMMC when it is not blanked), **game patches** (sys-patch or sigpatch files, with a warning recommending sys-patch when only files are used). The Overview repeats these two warnings. |

![Play timer](images/screenshots/play_timer.png)
![Per-day limits](images/screenshots/per_day.png)

### How the play-time limit is written safely

If the timer is counting down, overwriting its configuration destabilises Atmosphère. So before any write the app checks the state; if the timer is active, the confirmation dialog also says that parental controls are unlocked temporarily first (with the stored PIN — **you don't need to remember it**). One press then unlocks, checks that the system really reports the unlock, writes, and **locks again right away** (or offers to, if *Lock again automatically after a change* is off). The service layer re-checks the same state just before writing, so no screen can skip that safeguard.

`0` minutes means *no play that day*; *Remove the play-time limit* turns the timer off. ⚠️ Don't set a limit below the time already played today: as soon as parental controls are locked again, the game is suspended.

## Install

Pick one:

- **Homebrew App Store** or **sphaira's App Store** (same catalogue): search for *PlayGuard* once the listing is approved. New GitHub releases are picked up automatically.
- **sphaira's GitHub menu**: the release zip already contains the entry (`/config/sphaira/github/playguard.json`), so after a first install you can update from *GitHub* in sphaira.
- **Manually**: download `playguard.zip` from the [Releases](../../releases/latest) and extract it to the **root** of the SD card. The app lands in `sd:/switch/playguard/`.

Files the app writes: `sd:/switch/playguard/config.json` (preferences), `profiles/` (saved limits), `logs/` (diagnostics). More in [packaging/README.md](packaging/README.md).

### First steps

1. **Parental controls not set up yet:** System Settings › Parental Controls › set a PIN (skip the phone-app pairing). Then open the app.
2. **Already paired with the phone app:** open *Companion app* › *Unlink*, otherwise the next sync overwrites what you set here.
3. *Play timer* › *Same limit every day* (or *A different limit for each day…*).
4. If the network clock is not accurate (Overview), use *Network clock* › *Measure* then *Set the network clock* (enable *Synchronise Clock via Internet* in System Settings first).

Controls: ↑/↓ move, Ⓐ confirm, Ⓑ back (on the sidebar: press Ⓑ twice to exit), Ⓧ refresh.

## Bug reports

*Tools & about* › *Export a diagnostic report* saves a text file in `sd:/switch/playguard/logs/` with the firmware, Atmosphère version, clocks and the raw result of every parental-control query. It also gives the storage, the serial-blanking state and the game-patch status. **It never contains the PIN or the serial number.** Attach it to the issue.

A **read-only "PlayGuard Diagnostics" build** (`READ_ONLY=1`) cannot change anything; it is useful to investigate a new firmware safely.

## Build from source

```sh
make test        # unit tests of the C service layer (any gcc, no devkitPro)
make desktop     # the UI on Linux with a simulated console (needs GLFW / X11 / D-Bus dev packages)
make             # ./playguard.nro      (devkitPro switch-dev, DEVKITPRO set)
make dist        # ./playguard.zip      (SD-card layout)
make PROBE=1     # + diagnostic shortcuts in the Play timer tab
make READ_ONLY=1 # "PlayGuard Diagnostics" build
./run.sh [ip]    # build in the devkitpro/devkita64 Docker image, optionally nxlink to a console
```

The desktop build runs the real borealis UI against `source/sim/` (environment knobs: `PLAYGUARD_SIM_FW=20.5.0`, `PLAYGUARD_SIM_NO_CFW=1`, `PLAYGUARD_SIM_TIMER_OFF=1`, `PLAYGUARD_SIM_UNLOCKED=1`, `PLAYGUARD_SIM_UNPAIRED=1`, `PLAYGUARD_SIM_ACCURATE=1`, `PLAYGUARD_SIM_EMUMMC=1`, `PLAYGUARD_SIM_BLANK=1`; game patches are read from `./playguard_data/sd/`, the simulated SD card root). `tools/desktop_smoke.py` clicks through every screen headlessly; CI runs it with the unit tests, the resource checks (`tools/check_resources.py`) and the three Switch builds.

Branding: `branding/*.svg` (sources), rendered to `icon.jpg` and `images/store/*.png` by `node tools/render_branding.mjs` (Node + Playwright).

Layout: `source/core/` (C, libnx: `pctl_ops`, `time_ops`, `sysinfo`), `source/tab/` (one class per tab), `source/action/` (the play-timer write flow), `source/ui/` (dialogs, formatting, theme colours), `source/util/` (NTP, config, profiles, diagnostics), `resources/` (XML layouts, `i18n/en-US/playguard.json`, `i18n/fr/playguard.json`).

## Contributing

Releases are automated with [release-please](https://github.com/googleapis/release-please), which reads [conventional commits](https://www.conventionalcommits.org/):

- Give each pull request a conventional title (`feat: …`, `fix: …`, `docs: …`, `feat!: …` for a breaking change); CI checks it.
- **Squash-merge** pull requests, so each one lands as a single commit carrying that title. A plain merge commit makes release-please apply a PR's `BEGIN_COMMIT_OVERRIDE` block to every commit of the PR.
- release-please then keeps a `chore(main): release X.Y.Z` PR open; merging it tags the release and attaches the `.nro` / `.zip`.

## License

GPLv3 (see [`LICENSE`](LICENSE)). PlayGuard is maintained by **[JigSawFr](https://github.com/JigSawFr)**.

### Based on and credits

- PlayGuard is a fork of **Pctl Manager** by **Taylor** ([tailiang2008](https://github.com/tailiang2008)) (v2–v3): the original pctl service layer, play-timer write gate and borealis UI.
- UI: **[borealis](https://github.com/xfangfang/borealis)** (Apache 2.0), pinned at `extern/borealis/`.
- fw 22.5 diagnosis, session release and NTP synchronisation adapted from **[anbingxi/NX-Pctl-Manager](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly)**.
- Command reference: [switchbrew — Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).
