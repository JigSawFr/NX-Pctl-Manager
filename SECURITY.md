# Security policy

## Reporting a vulnerability

Report privately through GitHub, from the repository's **Security** tab:
[Report a vulnerability](https://github.com/JigSawFr/PlayGuard/security/advisories/new).
That opens a private advisory visible only to you and the maintainer. Please do not open a
public issue: PlayGuard guards a child's play time, and a way around it is useful to exactly
the people it is meant to hold back from the moment it is readable.

Expect an acknowledgement within a few days. A confirmed report is fixed in a new release,
and the advisory is published once that release is out.

Include, if you can: the firmware and Atmosphère versions, the PlayGuard version, what a
person gains, and the smallest sequence of steps that shows it.

## Supported versions

The latest release. Fixes are not backported.

## What is in scope

- **The PIN** — the PIN reaching a log, the diagnostic report, a backup, an export or the
  change history; or PlayGuard's own *ask for the PIN* prompt being skipped from the console
  itself (without editing the SD card).
- **Unsafe writes** — the play timer written while it counts down, a setting changed while
  read-only mode is on, or a write on a firmware PlayGuard has not been checked against
  unless *every feature at your own risk* was chosen on the firmware screen.
- **The recovery sysmodule** — acting without a `RESCUE` request file, acting twice on one
  request, or doing more than the request asks.
- **The update check and releases** — anything that makes PlayGuard trust a `compat.json`
  or a download that did not come from this repository's releases.

## What is not

- **Physical access to the SD card.** Whoever can edit the card can already turn off
  PlayGuard's own PIN prompt in `config.json`, install the recovery sysmodule, or remove
  parental controls with any pctl tool. See [`sysmodule/README.md`](sysmodule/README.md).
- **Custom firmware itself.** PlayGuard needs Atmosphère; a modded console can run any
  homebrew, including ones that change parental controls.
- **Nintendo's parental controls.** Behaviour of the system `pctl` service or the phone app
  belongs with Nintendo. PlayGuard is not affiliated with Nintendo.
