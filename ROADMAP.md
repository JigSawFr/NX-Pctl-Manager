# Roadmap

Where PlayGuard is going, and the ideas waiting for a decision. Nothing here
is a promise or a date: it is a list to pick from, most useful first in each
section. To propose something, open a
[feature request](https://github.com/JigSawFr/PlayGuard/issues/new?template=4-feature.yml);
to take something on, say so in an issue first.

The goal stays the same: everything the Nintendo Switch Parental Controls
phone app sets, on the console, offline, with nothing running in the
background. What that means feature by feature is in
[docs/companion-app.md](docs/companion-app.md); what is known of the console's
side is in [docs/parental-controls.md](docs/parental-controls.md).

## Waiting on data from a console

These need a finding before any code: most of them a play-timer block
compared before and after a change made from the phone app (*Developer tools ›
Compare the play-timer block*), on a console still linked to it. A report
attached to an issue is the most useful contribution right now.

- **Alarm only, or suspend the software** when the time is up. The setting
  parents ask for most after the limit itself; its byte in the block's header
  is not located yet.
  ([details](docs/companion-app.md#1-alarm-only-or-suspend-the-software))
- **Bedtime and start time per day of the week.** PlayGuard sets one bedtime
  for every day; the per-day bytes have never been seen with a bedtime on.
  ([details](docs/companion-app.md#2-bedtime-and-start-time-per-day-of-the-week))
- **Exceptions for specific software**: show the games on the
  free-communication list by name, then add and remove them, once the list's
  entries are decoded.
  ([details](docs/companion-app.md#3-exceptions-for-specific-software))
- **Firmware 23.0.1 / Atmosphère 1.12.0 tested on a console.** Covered by the
  command table, not yet run on hardware.

## Ready to build

No unknown left; only code.

- **A month in Activity**: per game, per account and per day over a month,
  with the same export, the offline counterpart of the phone app's monthly
  report. ([details](docs/companion-app.md#5-monthly-report))

## To investigate

- **Game categories and suggestions** in Activity: the console's control
  data names no genre, so "the category played most" or suggestions of new
  games would need an outside game database (downloaded, kept up to date).
  Worth it only if it stays optional and offline-friendly.
- **History of PIN entries** made from the HOME menu, as the phone app shows
  since 2.5.0: does the console log them anywhere PlayGuard can read?
  ([details](docs/companion-app.md#4-history-of-pin-entries))
- The open questions on the play timer (when the time spent resets on its
  own, whether it counts while temporarily unlocked, command 1406's error,
  1460's layout…): [docs/parental-controls.md](docs/parental-controls.md#still-open).
  Each answer makes PlayGuard's figures and warnings more exact.

## Ongoing

- **Translations by native speakers** of the 15 catalogues (see
  [CONTRIBUTING.md](CONTRIBUTING.md)).
- **The Homebrew App Store listing**, once approved.
- **New firmware**: PlayGuard opens read-only on an unknown firmware until a
  release supports it; each new firmware needs a check of the command table.

## Horizon

Further out, and bigger than a feature: PlayGuard drives the console's own
parental controls instead of replacing them, so the console can become one
piece of a home that already automates the rest. The first step would be a
**bridge to an MQTT broker**: the console pushes its state and statistics
(play time today and per game, the limit, bedtime, whether the timer is
counting, the restriction level) and pulls orders (extra time, no more play
today, switch profile, lock the console). From there, anything that speaks
MQTT can join in, Home Assistant first:

- **Switch profiles on their own** (*School week* / *Summer holidays*) from a
  calendar, school holidays, the weather (more play on a rainy day) or any
  other data at hand.
- **Chores that earn play time**: a chores app or add-on adds minutes when a
  task is done, or takes some away as a penalty.
- **Live figures elsewhere**: time left today on a dashboard, on a
  Busy Bar, on a phone.
- **Notifications that can be answered**: "10 minutes left" or "asks for more
  time", with a button that adds a quarter of an hour.
- **Several consoles at home** seen and set from one place.

What it must keep: the limit, the PIN, the warnings and the suspension stay
the console's own, so they keep working when the bridge, the network or the
broker is down. And it must hold to the rules the console sets: `pctl:a`
takes a single session, so a bridge opens it for each action and releases it
at once, as PlayGuard does, and never writes the play timer while it counts
down. Whether the bridge runs only while PlayGuard is open (sync on opening)
or as a separate, opt-in sysmodule that never takes over the controls is the
first decision to make; it would be the one exception to "nothing in the
background", and an optional one.

## Ideas without a decision

Asked for by parents (mostly in the phone app's store reviews). Each one runs
into a limit of the console's play timer, which PlayGuard drives but does not
replace; they stay here until someone finds a way that keeps "nothing in the
background".

| Idea | What stands in the way |
|---|---|
| Two play windows a day (morning, afternoon) | The timer has one bedtime and one "allowed again" time per day |
| A compulsory break after a stretch of play | The timer has no such mode; enforcing it needs something running while the game runs |
| A weekly budget instead of daily limits | The timer counts per day; moving unused time between days needs PlayGuard to run every day |
| A message shown on the console to the child | No system call for it outside a running homebrew |

## Not planned

| | Why |
|---|---|
| A limit per user account | The console's timer has one limit for the whole console. A replacement sysmodule does this (see the README's comparison) |
| Anything running in the background | PlayGuard only changes the console's own settings; the optional rescue sysmodule acts at boot, and only on request |
| Remote control through Nintendo's servers, a second parent's Nintendo Account | PlayGuard runs on the console, offline; remote control through a home server is the [horizon](#horizon) instead |
| GameChat settings | Nintendo Switch 2 only, which Atmosphère does not run on |
| Purchase restrictions | Nintendo Account settings, on Nintendo's servers |
