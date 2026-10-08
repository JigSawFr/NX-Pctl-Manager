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
| Firmware | **21.0.0 → 23.0.1** | The play-time limit layout (0x44 bytes) exists since 21.0.0. Below that, every tab works except the play timer. On a newer firmware the app starts **read-only** and looks for a PlayGuard release that supports it (see below). |
| Atmosphère | **1.11.x → 1.12.0** | 1.12.0 adds 23.0.0 support. The app shows the detected Atmosphère version. |
| Launchers | hbmenu, **sphaira**, **Homebrew App Store** | Launching over a game (title override) is recommended; the app shows whether it runs as an application or as an applet (album). |
| Hardware-tested | 22.1.0 / Atmosphère 1.11.1 | 23.0.1 / 1.12.0 is supported by the command table (switchbrew) but not yet tested on hardware: reports are welcome. |

**Newer firmware.** The *Firmware not supported yet* screen checks the latest release: when one supports the firmware, it offers to update with sphaira or the Homebrew App Store (Tools › *Update with*). Otherwise you continue read-only, read-only with the developer tools (to investigate the firmware), or with every feature at your own risk; the choice can be remembered for that firmware and app version. Tools › *Compatibility* brings the screen back.

**Why there are no 22.5 crashes.** `pctl:a`, the privileged parental-control service, accepts a **single session**. Older builds of the original app kept it open the whole time, so the HOME-menu PIN prompt (or the PIN applet) could not get it and Atmosphère could crash. PlayGuard opens the session for each action, does its work and releases it immediately; periodic refreshes pause while the app is in the background. *(Diagnosis by [anbingxi's fork](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly).)*

## Features

The app is organised in tabs, like System Settings.

| Tab | What you can do |
|---|---|
| **Overview** | Today first (a gauge of today's play time — while no game is running, the time played as the activity log counts it, marked ≈ —, today's limit, time left, bedtime alarm), then the parental-control state, PIN and restriction level, then what needs attention: network-clock accuracy, companion-app link (in amber while linked), firmware / compatibility only when there is a problem. Lines that open another tab end with a chevron (›); Ⓐ on *Today's limit* changes it right there, and Ⓐ on *Network clock › Inaccurate* offers to measure and set the clock on the spot. While no PIN is set, a **First steps** line opens the three-step guide (PIN, daily limit, network clock), which also comes up by itself at start-up. **Extra time today** (+15 min, +30 min, +1 h on today's limit; from the next day — at start-up, or at midnight if the app is open — it offers to put the usual limit back, or does it by itself with *Preferences › Put the usual limit back the next day by itself*; until then a line on the Overview does it). **No more play today** sets today's limit to 0 for today only (the game in progress is suspended at the lock, which the confirmation says), and the usual limit comes back the next day the same way. A banner with **Lock now** appears while parental controls are temporarily unlocked. Refreshes every 5 s and shows the time of the last refresh (Ⓧ refreshes now). |
| **Play timer** | One state line (active, time left, today's limit, the matching profile; a red line while the limit is reached), then **the week chart, which is the editor**: ←/→ pick a day, Ⓐ changes its limit. Same limit every day (quick list or any value, typed in minutes — `90`, `90 min` — or as `1:30`, `1h30`), a different limit per day (the same chart with unsaved days in amber, quick values, Monday–Friday / weekend presets, "no limit" per day, the **+** button saves from anywhere), remove the limit, **extra time today** and **no more play today** (as on the Overview), a **Profiles** screen for limits saved on the SD card (e.g. *School week*, *Vacances d'été*: Ⓐ on one to apply it, edit the limits or rename without applying, or delete it; save the current limits or make a new one; any name, accents included — two names that would be one file on the SD card are caught before one replaces the other), bedtime alarm (read-only). Every confirmation of a change draws the week as it will be, the days that change in amber. Advanced, opt-in: "time's up" alarm on/off, pause / resume the countdown. |
| **Activity** | How long each game was played **today**, in the **last 7 days** and **in all**, for every account or **one user account** (*Account*, when the console has several), from the console's own activity log, with a chart of the last seven days (with each day's limit as a line and the time over it in amber, for all accounts) and today's, the week's and the all-time totals. Sort the list by period; the first games carry their icon (not in applet mode, to spare memory); Ⓐ on a game opens its own screen: its last seven days as bars, launches, first and last play and the time of each user account. Deleted games keep their all-time figures. **Export to the SD card** as CSV, JSON, XLSX (Excel) or PDF, with one column per day. Times are approximate if the console clock was changed. |
| **Restrictions** | Restriction level (None, Young child, Child, Teen, Custom) — what the chosen preset restricts (age limit, posting, communication) is shown read-only, and the picker says it before choosing; in Custom: age rating, social-media posting, communication with others; VR mode; **rating organisation** (PEGI, ESRB, USK, CERO …: the body the age limit is read with); the number of games allowed to communicate (the list itself is in the diagnostic report, raw, until its layout is known). |
| **Network clock** | Console / network clocks, time zone, accuracy. Pick a public NTP server (≈ 50 built-in, by region, or your own), **measure** against 3 servers (a spinner meanwhile; median, an amber warning when they disagree) and **set the network clock** (a measurement stays usable for 2 minutes, with a countdown). The play timer relies on this clock; a console that never reaches Nintendo's servers keeps it inaccurate. |
| **Security & app** | Set / change the PIN (system PIN screen), **ask for the PIN** in PlayGuard itself (never, *before a change* — anyone can look, only the parent changes something, asked again after 5 min — or *to open PlayGuard*; checked in the service layer so no change skips it; locking again never asks; the setting is on the SD card), **show the PIN** (after a warning, for when it is forgotten), unlock temporarily, **lock now**. *Companion app*: whether the Nintendo Switch Parental Controls phone app is linked, last synchronisation, unlink it (otherwise its next sync overwrites the limits set here). Delete all parental controls (its line in red; two confirmations with different red buttons, irreversible; a backup of the settings is saved first). |
| **Preferences** | Language and theme (with an offer to restart), the tab to start on, **lock again automatically after a change** (on by default), the extra-time amounts (+15/+30/+1 h, +10/+20/+30 min …), putting the usual limit back the next day by itself, a network-clock check at start-up (a toast when it is more than a minute off; it never sets the clock), advanced actions. |
| **Tools & about** | **Change history**: what PlayGuard changed on the console (limits, restriction level, PIN, unlocks, unlinking, the clock, restores …), when and from where (*Extra time today*, a profile …), the newest 200 kept on the SD card; Ⓐ on a change shows it and, for a value, **puts the previous one back** (through the same unlock and PIN as any change, saying when it changed since). **Back up / restore the settings** on the SD card (restriction level, custom settings, VR mode, rating organisation, daily limits, the "time's up" alarm — restored with the advanced actions on; for the record: the raw play-timer block; not the PIN; the restore lists only what would change), with how many backups to keep. *First steps* opens the guide again (with an *Unlink the companion app* step while it is linked, and a switch to stop it coming up at start-up). *Updates*: **check for updates** (now, or once a day at start-up; the date of the last check), *Update with*: sphaira, Homebrew App Store or by hand. Export a diagnostic report. *Console*: firmware, Atmosphère, compatibility, **storage** (emuMMC or sysMMC), whether Atmosphère **blanks the serial number** (with the number the system sees, partly hidden until Ⓐ; a warning on emuMMC when it is not blanked), **game patches** (sys-patch or sigpatch files, with a warning recommending sys-patch when only files are used). The Overview repeats these two warnings. |

![Play timer](images/screenshots/play_timer.png)
![Per-day limits](images/screenshots/per_day.png)
![Activity](images/screenshots/activity.png)
![A game](images/screenshots/activity_game.png)

### How the play-time limit is written safely

If the timer is counting down, overwriting its configuration destabilises Atmosphère. So before any write the app checks the state; if the timer is active, the confirmation dialog also says that parental controls are unlocked temporarily first (with the stored PIN — **you don't need to remember it**). One press then unlocks, checks that the system really reports the unlock, writes, and **locks again right away** (or offers to, if *Lock again automatically after a change* is off). The service layer re-checks the same state just before writing, so no screen can skip that safeguard.

`0` minutes means *no play that day*; *Remove the play-time limit* turns the timer off. ⚠️ A limit below the time already played today suspends the game as soon as parental controls are locked again; the confirmation says so when that would happen.

## Install

Pick one:

- **Homebrew App Store** or **sphaira's App Store** (same catalogue): search for *PlayGuard* once the listing is approved. New GitHub releases are picked up automatically.
- **sphaira's GitHub menu**: the release zip already contains the entry (`/config/sphaira/github/playguard.json`), so after a first install you can update from *GitHub* in sphaira.
- **Manually**: download `playguard.zip` from the [Releases](../../releases/latest) and extract it to the **root** of the SD card. The app lands in `sd:/switch/playguard/`.

Files the app writes: `sd:/switch/playguard/config.json` (preferences), `history.json` (the change history), `profiles/` (saved limits), `backups/` (settings backups), `exports/` (Activity exports), `logs/` (diagnostics). More in [packaging/README.md](packaging/README.md).

### First steps

1. **Parental controls not set up yet:** open the app: the *First steps* screen sets the PIN (the system PIN screen), the daily limit, and checks the network clock.
2. **Already paired with the phone app:** open *Security & app* › *Unlink the companion app*, otherwise the next sync overwrites what you set here.
3. *Play timer* › *Same limit every day* (or *A different limit for each day…*).
4. If the network clock is not accurate (Overview), use *Network clock* › *Measure* then *Set the network clock* (enable *Synchronise Clock via Internet* in System Settings first).

Controls: ↑/↓ move, Ⓐ confirm, Ⓑ back or cancel (on the sidebar: press Ⓑ twice to exit), Ⓧ refresh, **+** saves the per-day limits. Only status lines are skipped by the focus; what failed is said in a dialog, what worked in a toast. The title says when the app is read-only and while parental controls are temporarily unlocked; in read-only mode the actions stay in place, greyed, and say why when pressed.

## Locked out? (second-hand console, forgotten PIN)

PlayGuard needs Atmosphère, and it only sees the parental controls of the system it runs on: **emuMMC and sysMMC each have their own** (a PIN removed on one is still there on the other). Run it on each one that needs fixing.

| Situation | What to do |
|---|---|
| **Second-hand console: you know the PIN, but the previous owner's phone app is still linked** (unlinking fails, or a factory reset asks for their account) | *Security & app* › *Unlink the companion app*, then, if you want no parental controls at all, *Delete all parental controls*. Both work offline, on emuMMC as on sysMMC. |
| **PIN forgotten** | *Security & app* › *Show the PIN*. Or *Delete all parental controls* to start again (a backup of the settings is saved first; it never contains the PIN). |
| **PIN forgotten, and *Ask for the PIN* is set to *To open PlayGuard* or *Before a change*** | That setting is in `sd:/switch/playguard/config.json`, on purpose: put the SD card in a computer and set `"pin_lock"` to `"off"`. |
| **Console not modded** | PlayGuard cannot help: it needs Atmosphère. Nintendo support's master-key procedure is the official way. |

## Bug reports

*Tools & about* › *Export a diagnostic report* saves a text file in `sd:/switch/playguard/logs/` with the firmware, Atmosphère version, clocks and the raw result of every parental-control query. It also gives the storage, the serial-blanking state and the game-patch status. **It never contains the PIN or the serial number.** Attach it to the issue.

**Developer mode** (press *Tools & about* › *Version* seven times) adds a read-only switch, the diagnostic report on screen and a shortcut to export it from the Play timer tab. With read-only on, the app cannot change anything: that is the safe way to investigate a new firmware (the firmware screen offers it directly). *Compare the play-timer block* helps decode the play-timer settings PlayGuard does not show yet. Save the raw block as a reference, change one setting in the Nintendo Switch Parental Controls app, then come back: PlayGuard lists the values that changed (in `logs/` on request). Settings it could find this way include bedtime and "alarm only" vs "suspend the software". Attach that file to an issue.

## Build from source

```sh
make test        # unit tests of the C service layer (any gcc, no devkitPro; ASan + UBSan, SAN= to turn off)
make desktop     # the UI on Linux with a simulated console (needs GLFW / X11 / D-Bus dev packages)
make             # ./playguard.nro      (devkitPro switch-dev, DEVKITPRO set; drawn with deko3d, GL=1 for OpenGL)
make dist        # ./playguard.zip      (SD-card layout)
./run.sh [ip]    # build in the devkitpro/devkita64 Docker image, optionally nxlink to a console
```

The desktop build runs the real borealis UI against `source/sim/` (environment knobs: `PLAYGUARD_SIM_FW=20.5.0`, `PLAYGUARD_SIM_NO_CFW=1`, `PLAYGUARD_SIM_TIMER_OFF=1`, `PLAYGUARD_SIM_UNLOCKED=1`, `PLAYGUARD_SIM_UNPAIRED=1`, `PLAYGUARD_SIM_ACCURATE=1`, `PLAYGUARD_SIM_EMUMMC=1`, `PLAYGUARD_SIM_BLANK=1`, `PLAYGUARD_SIM_NO_PDM=1`, `PLAYGUARD_SIM_LATEST=1.1.0:24.0.0` or `offline` for the update check, `PLAYGUARD_SIM_NUMPAD=1:30` for what the system number pad returns, `PLAYGUARD_SIM_HBLOADER=1` to hand an update over to a store, `PLAYGUARD_SIM_REGION=2` for the console region, `PLAYGUARD_SIM_BACKGROUND=1` for PlayGuard out of focus, `PLAYGUARD_SIM_IDLE=1` for no game running, `PLAYGUARD_SIM_NOW=<POSIX seconds>` for a frozen console time (with `TZ=` for its time zone); game patches are read from `./playguard_data/sd/`, the simulated SD card root). `tools/desktop_smoke.py` clicks through every screen headlessly (`gate` scenario: the firmware screen and developer mode on a simulated 24.0.0); CI runs it with the unit tests, the resource checks (`tools/check_resources.py`) and the Switch build, and compares its screens with the references in `tests/visual/` (`tools/visual_check.py`; see `tests/visual/README.md` to update them). Each release also publishes `compat.json` (`tools/gen_compat.py`: version and newest checked firmware), which the app's update check reads.

Branding: `branding/*.svg` (sources), rendered to `icon.jpg` and `images/store/*.png` by `node tools/render_branding.mjs` (Node + Playwright).

Layout: `source/core/` (C, libnx: `pctl_ops`, `time_ops`, `sysinfo`, `playstats`), `source/tab/` (one class per tab), `source/action/` (the play-timer write flow, the clock flow, the settings restore, the firmware screen, updates), `source/activity/` (the screens: per-day editor, profiles, a game, first steps, change history, firmware), `source/view/` (the week chart, the gauge, the day bars, the game cell), `source/ui/` (dialogs, formatting, theme colours), `source/util/` (NTP, config, profiles, settings backups, change history, play-log folding, table export, diagnostics, update check, store launcher), `resources/` (XML layouts, `i18n/<language>/playguard.json`).

## Contributing

Releases are automated with [release-please](https://github.com/googleapis/release-please), which reads [conventional commits](https://www.conventionalcommits.org/):

- Give each pull request a conventional title (`feat: …`, `fix: …`, `docs: …`, `feat!: …` for a breaking change); CI checks it.
- **Squash-merge** pull requests, so each one lands as a single commit carrying that title. A plain merge commit makes release-please apply a PR's `BEGIN_COMMIT_OVERRIDE` block to every commit of the PR.
- release-please then keeps a `chore(main): release X.Y.Z` PR open; merging it tags the release and attaches the `.nro` / `.zip`.

### Translating PlayGuard

1. Copy `resources/i18n/en-US/playguard.json` to `resources/i18n/<code>/playguard.json` (`<code>` as the console names its language: `de`, `es`, `it`, `pt-BR`, `zh-Hans` …) and translate the values. Keep every key, and as many `{}` placeholders as the English text, in the same order.
2. Add `<code>` to `config::LANGUAGES` in `source/util/config.hpp` (the language picker and the saved preference use that list).
3. Add the language's own name under `"tools" › "languages"` in **every** `playguard.json` (`"de": "Deutsch"`).
4. Optionally add `resources/i18n/<code>/hints.json` for borealis' button hints, when borealis has none for that language (see `resources/i18n/fr/hints.json`); missing strings fall back to English.
5. Run `python3 tools/check_resources.py .`: it reports missing or extra keys, placeholder mismatches and a language missing from the picker. CI runs the same check.

## License

GPLv3 (see [`LICENSE`](LICENSE)). PlayGuard is maintained by **[JigSawFr](https://github.com/JigSawFr)**.

### Based on and credits

- PlayGuard is a fork of **Pctl Manager** by **Taylor** ([tailiang2008](https://github.com/tailiang2008)) (v2–v3): the original pctl service layer, play-timer write gate and borealis UI.
- UI: **[borealis](https://github.com/xfangfang/borealis)** (Apache 2.0), pinned at `extern/borealis/`.
- fw 22.5 diagnosis, session release and NTP synchronisation adapted from **[anbingxi/NX-Pctl-Manager](https://github.com/anbingxi/NX-Pctl-Manager/tree/diag/fw22-5-readonly)**.
- Command reference: [switchbrew — Parental Control services](https://switchbrew.org/wiki/Parental_Control_services).
