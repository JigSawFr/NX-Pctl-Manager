# Homebrew App Store (4TU) listing

Submission form: <https://submit.fortheusers.org/> (the same catalogue is shown by sphaira's App Store).

| Field | Value |
|---|---|
| Title (3–24 chars) | `PlayGuard` |
| App URL | `https://github.com/JigSawFr/PlayGuard` |
| Category | Tools |
| License | GPLv3 |
| Icon (256×150) | [`images/store/icon.png`](../../images/store/icon.png) |
| Banner (848×208) | [`images/store/banner.png`](../../images/store/banner.png) |
| Screenshots | [`images/screenshots/`](../../images/screenshots) |
| Package | release asset `playguard.zip` → `switch/playguard/playguard.nro` |

Short description:

> Offline parental controls for CFW consoles: daily play-time limit, restrictions, network clock and PIN — no phone app needed.

Long description:

> PlayGuard brings the Nintendo Switch parental-control settings that normally need the phone app onto the console, fully offline. Set one daily play-time limit or one per day, save limit profiles, change the restriction level, age rating, social-media and communication restrictions, set the network clock from a public NTP server, set or reset the PIN, unlock or re-lock temporarily, unlink the companion app, or delete everything when the PIN is forgotten. Requires Atmosphère. Play-time limit on firmware 21.0.0 to 23.0.1; the other features also work on older firmware. English and French.

## Updates

Once listed, new GitHub releases are detected automatically and published after a manual review. Releases are automated with [release-please](https://github.com/googleapis/release-please):

1. Merge pull requests with a conventional-commit title (`feat: …`, `fix: …`; checked by `.github/workflows/pr-title.yml`).
2. release-please keeps a `chore(main): release X.Y.Z` PR open with the next version, the `CHANGELOG.md` entry and the bumped `VERSION_*` lines of `CMakeLists.txt`.
3. Merging that PR creates the `vX.Y.Z` tag and the GitHub release; `.github/workflows/release-please.yml` then builds and attaches `playguard.nro`, `playguard.zip`, `compat.json`, `build-info.txt` and `SHA256SUMS.txt`.
