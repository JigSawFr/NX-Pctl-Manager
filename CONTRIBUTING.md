# Contributing to PlayGuard

Thanks for helping! This file covers building, testing, the code layout, releases and translations. For what the app does, see the [README](README.md); for what to work on, the [roadmap](ROADMAP.md).

## Build from source

| Command | What it does | Needs |
|---|---|---|
| `make test` | Unit tests of the C service layer, with ASan + UBSan (`SAN=` to turn them off) | Any gcc — no devkitPro |
| `make desktop` | The real UI on Linux, against a simulated console | GLFW / X11 / D-Bus dev packages |
| `make` | `./playguard.nro` — drawn with deko3d (`GL=1` for OpenGL) | devkitPro `switch-dev`, `DEVKITPRO` set |
| `make dist` | `./playguard.zip` (SD-card layout) | as above |
| `make dist-rescue` | `./playguard-rescue.zip`, the optional recovery sysmodule (`make` also builds it and bundles it in the romfs, for *Tools › Optional modules*) | as above |
| `make dist-agent` | `./playguard-agent.zip`, the optional remote-link agent (bundled the same way) | as above |
| `./run.sh [ip]` | Builds in the `devkitpro/devkita64` Docker image, optionally `nxlink`s to a console | Docker |

Clone with submodules (`git clone --recursive`, or `git submodule update --init`): borealis is pinned at `extern/borealis/`.

## The desktop simulator

`make desktop` runs the real borealis UI against `source/sim/`, a simulated console. Environment knobs:

| Variable | Simulates |
|---|---|
| `PLAYGUARD_SIM_FW=20.5.0` | Another firmware (default 23.0.1) |
| `PLAYGUARD_SIM_NO_CFW=1` | No Atmosphère |
| `PLAYGUARD_SIM_NOT_SET_UP=1` | Parental controls never set up (no PIN) |
| `PLAYGUARD_SIM_TIMER_OFF=1` | No play-time limit |
| `PLAYGUARD_SIM_RESTRICTED=1` | Today's limit reached |
| `PLAYGUARD_SIM_UNLOCKED=1` | Parental controls temporarily unlocked |
| `PLAYGUARD_SIM_UNPAIRED=1` | No companion app linked |
| `PLAYGUARD_SIM_ALARM_OFF=1` | The "time's up" alarm off |
| `PLAYGUARD_SIM_ACCURATE=1` | An accurate network clock |
| `PLAYGUARD_SIM_AUTOSYNC_OFF=1` | *Synchronise Clock via Internet* off |
| `PLAYGUARD_SIM_FAIL=timer,clock,unverified,bedtime` | Failures: the play-timer write, setting the clock, an unlock the system does not confirm, a bedtime the console does not take |
| `PLAYGUARD_SIM_EMUMMC=1` | Running on emuMMC |
| `PLAYGUARD_SIM_BLANK=1` | Atmosphère blanking the serial number |
| `PLAYGUARD_SIM_APPLET=1` | Applet (album) mode |
| `PLAYGUARD_SIM_NO_PDM=1` | No activity log |
| `PLAYGUARD_SIM_IDLE=1` | No game running |
| `PLAYGUARD_SIM_BACKGROUND=1` | PlayGuard out of focus |
| `PLAYGUARD_SIM_REGION=2` | The console region |
| `PLAYGUARD_SIM_LATEST=1.1.0:24.0.0` | The latest release and the newest firmware it supports, for the update check (`offline` for no network) |
| `PLAYGUARD_SIM_PASTE=https://dpaste.org/AbC1` | What dpaste.org answers to *Send a report online* (`offline` for no network) |
| `PLAYGUARD_SIM_DEV_BUILDS=<folder>` | What the GitHub API returns for *Install another build*: `latest.json` (the latest release), `artifacts.json`, `pulls.json`; `https://<local path>` download URLs are copied from disk (`offline` for no network) |
| `PLAYGUARD_SIM_GITHUB_LOGIN=ok` | The GitHub device flow of *GitHub account*: `ok` approves at once, `denied` refuses, `offline` fails |
| `PLAYGUARD_SIM_HBLOADER=1` | A homebrew loader that can hand an update over to a store |
| `PLAYGUARD_SIM_NUMPAD=1:30` | What the system number pad returns |
| `PLAYGUARD_SIM_NOW=<POSIX seconds>` | A frozen console time (with `TZ=` for its time zone) |
| `PLAYGUARD_SIM_MODULES=agent:running` | Optional modules running at start (`name:running`, comma-separated); a module starts only when its `exefs.nsp` is on the simulated SD card |
| `PLAYGUARD_SIM_AGENT=running` | The agent sysmodule runs while its simulated module does (`PLAYGUARD_SIM_MODULES=agent:running`): an agent simulated in the process, with its own command dispatch (`sysmodule/agent/source/agent_core.c`), to which PlayGuard hands the link; what it pushes and answers is logged as `sim agent: …`. An installed `exefs.nsp` holding `old agent` speaks another protocol, one holding `broken agent` never answers |
| `PLAYGUARD_SIM_AGENT_ORDER=limit_uniform=90` | An order the simulated agent hands PlayGuard once, after `Hello` |
| `PLAYGUARD_SIM_BUNDLED=<folder>` | The optional modules PlayGuard carries (`<name>/exefs.nsp` and `version.txt`, as `cmake/bundle_sysmodules.cmake` writes them in the romfs) |

Game patches are read from `./playguard_data/sd/`, the simulated SD card root.

## Tests and CI

- `make test` — host unit tests (`tests/`, see [`tests/README.md`](tests/README.md)), including the recovery sysmodule logic (`tests/rescue/`), the remote link (`tests/sync_*`) and the agent (`tests/agent/`).
- `make check` — `make test`, then the resource check, the remote link's JSON documents (`tools/check_sync_json.py`) and `compat.json`.
- `tools/desktop_smoke.py <out-dir> [gate|errors|rescue|devbuild|sync]` — clicks through every screen of the desktop build headlessly (needs `DISPLAY`, `xdotool`, ImageMagick) and saves screenshots. The `gate` scenario covers the firmware screen and developer mode on a simulated 24.0.0; `errors` covers a failed unlock and an unsettable clock; `rescue` the recovery screen; `devbuild` signing in to a simulated GitHub and installing a pull request's build in place out of its artifact; `sync` the remote link against a local Mosquitto (needs `mosquitto` and `mosquitto-clients`): what it publishes, an order applied and one refused, Home Assistant restarting, and `offline` at exit; `modules` installing, turning off at boot and removing the recovery module.
- `tools/visual_check.py` — compares those screenshots with the references in `tests/visual/`. A difference is reported as a warning, not a failure; see [`tests/visual/README.md`](tests/visual/README.md) to update them.
- `python3 tools/check_resources.py .` — checks the XML layouts and translation catalogs.

CI runs all of the above plus the Switch build.

## Code layout

| Path | Content |
|---|---|
| `source/core/` | C, libnx: `pctl_ops`, `time_ops`, `sysinfo`, `playstats`, `rescue`, `modules_nx` (start / stop a sysmodule) |
| `source/tab/` | One class per tab |
| `source/action/` | Flows: play-timer write, clock, settings restore, firmware screen, updates, the remote link (`sync_flow`, `sync_orders`) |
| `source/activity/` | Screens: per-day editor, profiles, a game, first steps, change history, firmware |
| `source/view/` | Widgets: the week chart, the gauge, the day bars, the game cell |
| `source/ui/` | Dialogs, formatting, theme colours |
| `source/util/` | NTP, config, profiles, the optional modules on the SD card, settings backups, change history, play-log folding, the play-data cache, table export, diagnostics, sending reports online, update check, store launcher |
| `source/sync/` | C, no allocation: the optional remote link (MQTT client, Home Assistant discovery, orders and their execution), shared with the agent sysmodule. See [`docs/sync-design.md`](docs/sync-design.md) |
| `source/sim/` | The simulated console for the desktop build |
| `resources/` | XML layouts and `i18n/<language>/playguard.json` |
| `sysmodule/` | The optional sysmodules, built with `sysmodule/common.mk`: `rescue/` (recovery at boot; shares `source/core/rescue.c` with the app) and `agent/` (the remote link in the background; links `source/core/` and `source/sync/`). See [`sysmodule/README.md`](sysmodule/README.md) |
| `packaging/` | Store and sphaira entries. See [`packaging/README.md`](packaging/README.md) |
| `branding/` | SVG sources of the icon and banners |
| `docs/` | [`parental-controls.md`](docs/parental-controls.md): what is known of the parental-control service, the play-timer block and the clocks, and how sure each fact is; [`companion-app.md`](docs/companion-app.md): what PlayGuard covers of Nintendo's phone app, and the gaps still to close; [`sync-design.md`](docs/sync-design.md) and [`sync-protocol.md`](docs/sync-protocol.md): the design and the wire contract of the optional remote link (MQTT, Home Assistant): PlayGuard's side is implemented, the agent sysmodule is not yet; [`home-assistant.md`](docs/home-assistant.md) (and `.fr.md`): the user guide |

**Branding:** `branding/*.svg` are rendered to `icon.jpg` and `images/store/*.png` by `node tools/render_branding.mjs` (Node + Playwright).

## Pull requests and releases

Releases are automated with [release-please](https://github.com/googleapis/release-please), which reads [conventional commits](https://www.conventionalcommits.org/):

- Give each pull request a conventional title (`feat: …`, `fix: …`, `docs: …`, `feat!: …` for a breaking change); CI checks it.
- **Squash-merge** pull requests, so each one lands as a single commit carrying that title. A plain merge commit makes release-please apply a PR's `BEGIN_COMMIT_OVERRIDE` block to every commit of the PR.
- release-please keeps a `chore(main): release X.Y.Z` PR open; merging it tags the release and attaches the `.nro` / `.zip`.

**Development builds:** nothing is published for them. The developer tools install the `playguard_release` artifact of a build run (a zip holding `playguard.nro`): main's last commits and each open pull request's newest run, forks included (`source/util/dev_builds.hpp`). GitHub hands artifacts to signed-in users only, hence the GitHub sign-in (device flow, OAuth app client ID in `source/util/github_auth.hpp`, no scope). For a pull request, `PLAYGUARD_COMMIT` makes the app report its head commit, as the artifacts API names the run.

Each release also publishes `compat.json` (`tools/gen_compat.py`: the version and the newest checked firmware), which the app's update check reads, plus `build-info.txt` and `SHA256SUMS.txt`. Details in [`packaging/README.md`](packaging/README.md). The `.nro`, both `.zip` and `compat.json` also carry a [build provenance attestation](https://docs.github.com/actions/security-for-github-actions/using-artifact-attestations): `gh attestation verify playguard.nro -R JigSawFr/PlayGuard` checks that a download was built by this repository's CI.

When a change is visible to users, update **both** [README.md](README.md) and [README.fr.md](README.fr.md).

## Translating PlayGuard

PlayGuard is translated into every language the console offers: English, French (France and Canada), German, Spanish (Spain and Latin America), Italian, Dutch, Portuguese (Portugal and Brazil), Russian, Japanese, Korean and Chinese (simplified and traditional). It follows the console's language unless *Preferences › Language* picks another one. British English consoles get the English strings, already in British spelling (`CMakeLists.txt` copies them under `en-GB`). Fixes from native speakers are welcome.

To add a language:

1. Copy `resources/i18n/en-US/playguard.json` to `resources/i18n/<code>/playguard.json` (`<code>` as the console names its language, see `brls::LOCALE_*`) and translate the values. Keep every key, and as many `{}` placeholders as the English text, in the same order.
2. Add `<code>` to `config::LANGUAGES` in `source/util/config.hpp` (the language picker and the saved preference use that list).
3. Add the language's own name under `"tools" › "languages"` in **every** `playguard.json` (`"ru": "Русский"`). A regional variant (`fr-CA`, `es-419`, `pt-BR`) is a full catalog of its own: borealis falls back to English, not to the base language.
4. Optionally add `resources/i18n/<code>/hints.json` for borealis' button hints when borealis has none for that language (see `resources/i18n/fr/hints.json`); missing strings fall back to English.
5. Run `python3 tools/check_resources.py .`: it reports missing or extra keys, placeholder mismatches and a language missing from the picker. CI runs the same check.
