# Installing and updating

The release zip (`playguard.zip`) is meant to be extracted at the root of the SD card:

```
switch/playguard/playguard.nro     the app (hbmenu, sphaira, hb-appstore)
switch/playguard/LICENSE.txt       the GPLv3 text
config/sphaira/github/playguard.json     sphaira "GitHub" updater entry
```

At runtime the app writes, next to itself:

```
switch/playguard/config.json             preferences (language, theme, NTP server, …)
switch/playguard/profiles/*.json         saved play-time limit profiles
switch/playguard/backups/*.json          settings backups (never contain the PIN)
switch/playguard/exports/activity_*      Activity exports (CSV, JSON, XLSX, PDF)
switch/playguard/logs/*.txt              diagnostic reports (never contain the PIN), crash.txt, uploads.txt (links of reports sent online)
switch/playguard/logs/play_timer_log.csv   Developer tools › record the play timer (.old.csv: the previous one)
switch/playguard/logs/play_timer_block.json   Developer tools › play-timer block reference
switch/playguard/history.json            the change history (the newest 200 changes)
switch/playguard/cache/                  play data per account (activity_*.bin), the development build list (dev_builds.json)
switch/playguard/github_token            GitHub sign-in of the developer tools: the token in plain text,
                                         treat it as a password (Disconnect deletes it)
switch/playguard/rescue_report.txt       left by the recovery sysmodule, read at start-up
```

The optional recovery sysmodule (`playguard-rescue.zip`, a separate download) is also extracted at the root of the SD card:

```
atmosphere/contents/4200000000505247/exefs.nsp          the sysmodule (see sysmodule/README.md)
atmosphere/contents/4200000000505247/flags/boot2.flag   started by Atmosphère at boot
atmosphere/contents/4200000000505247/LICENSE.txt        the GPLv3 text
```

## Channels

| Channel | How it finds updates |
|---|---|
| Homebrew App Store (4TU) and sphaira's App Store | The listing follows the GitHub releases of this repository and picks up the `.zip` asset (see [hb-appstore/README.md](hb-appstore/README.md)). |
| sphaira › GitHub | Reads [`sphaira/playguard.json`](sphaira/playguard.json): downloads the `playguard.nro` asset of the latest release into `/switch/playguard/`. |
| Manual | Extract the zip at the SD card root. |

Release assets are produced by `.github/workflows/build.yml`, called by `release-please.yml` when a release PR is merged: `playguard.nro`, `playguard.zip`, `playguard-rescue.zip` (the optional recovery sysmodule), `compat.json` (the version and the newest checked firmware, read by the app's update check from `releases/latest/download/compat.json`), `build-info.txt` (the commit, the submodules and the devkitPro image and packages used), `SHA256SUMS.txt`. The Switch job runs in the `devkitpro/devkita64` image pinned by digest (in `build.yml`, and the same digest in `run.sh` for local builds), so a release can be rebuilt with the same toolchain; it fails on a libnx older than 4.12.0 or an `.nro` without libnx's `LNY2` marker (`tools/check_nro.py`). The job refuses to publish when the tag differs from the version in `CMakeLists.txt` (which is also the NACP version the stores display).
