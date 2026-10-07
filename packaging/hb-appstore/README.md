# Homebrew App Store (4TU) listing

Submission form: <https://submit.fortheusers.org/> (the same catalogue is shown by sphaira's App Store).

| Field | Value |
|---|---|
| Title (3–24 chars) | `Pctl Manager` |
| App URL | `https://github.com/JigSawFr/NX-Pctl-Manager` |
| Category | Tools |
| License | GPLv3 |
| Icon (256×150) | [`images/store/icon.png`](../../images/store/icon.png) |
| Banner (848×208) | [`images/store/banner.png`](../../images/store/banner.png) |
| Screenshots | [`images/screenshots/`](../../images/screenshots) |
| Package | release asset `nx_pctl_manager.zip` → `switch/nx_pctl_manager/nx_pctl_manager.nro` |

Short description:

> Offline parental controls for CFW consoles: daily play-time limit, restrictions, network clock and PIN — no phone app needed.

Long description:

> Pctl Manager brings the Nintendo Switch parental-control settings that normally need the phone app onto the console, fully offline. Set one daily play-time limit or one per day, save limit profiles, change the restriction level, age rating, social-media and communication restrictions, set the network clock from a public NTP server, set or reset the PIN, unlock or re-lock temporarily, unlink the companion app, or delete everything when the PIN is forgotten. Requires Atmosphère. Firmware 21.0.0 to 23.0.1. English and French.

## Updates

Once listed, new GitHub releases are detected automatically and published after a manual review. Release checklist:

1. Bump `VERSION_MAJOR/MINOR/ALTER` in `CMakeLists.txt` and add a `CHANGELOG.md` entry.
2. Tag `vX.Y.Z` (same version) and push the tag.
3. CI builds, checks the tag against the version and attaches `nx_pctl_manager.nro`, `nx_pctl_manager.zip` and `SHA256SUMS.txt` to the release.
