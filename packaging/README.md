# Installing and updating

The release zip (`nx_pctl_manager.zip`) is meant to be extracted at the root of the SD card:

```
switch/nx_pctl_manager/nx_pctl_manager.nro     the app (hbmenu, sphaira, hb-appstore)
config/sphaira/github/nx_pctl_manager.json     sphaira "GitHub" updater entry
```

At runtime the app writes, next to itself:

```
switch/nx_pctl_manager/config.json             preferences (language, theme, NTP server, …)
switch/nx_pctl_manager/profiles/*.json         saved play-time limit profiles
switch/nx_pctl_manager/logs/*.txt              diagnostic reports (never contain the PIN)
```

## Channels

| Channel | How it finds updates |
|---|---|
| Homebrew App Store (4TU) and sphaira's App Store | The listing follows the GitHub releases of this repository and picks up the `.zip` asset (see [hb-appstore/README.md](hb-appstore/README.md)). |
| sphaira › GitHub | Reads [`sphaira/nx_pctl_manager.json`](sphaira/nx_pctl_manager.json): downloads the `nx_pctl_manager.nro` asset of the latest release into `/switch/nx_pctl_manager/`. |
| Manual | Extract the zip at the SD card root. |

Release assets are produced by `.github/workflows/build.yml` on a `vX.Y.Z` tag: `nx_pctl_manager.nro`, `nx_pctl_manager.zip`, `SHA256SUMS.txt`. The job refuses to publish when the tag differs from the version in `CMakeLists.txt` (which is also the NACP version the stores display).
