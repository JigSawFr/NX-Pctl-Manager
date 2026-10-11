# playguard-rescue — the recovery sysmodule

A tiny Atmosphère boot sysmodule: the **last chance** for a parent who set a
play-time limit of 0 minutes (or any limit that suspends every program) and
then **forgot the PIN**. When the timer blocks everything, the HOME menu, the
Album and PlayGuard itself cannot start — so no homebrew can undo it. This
module runs before any of that, from the SD card.

It is **optional** and ships as its own download (`playguard-rescue.zip`), never
as part of the normal PlayGuard install. Install it only if you want this safety
net. PlayGuard works fully without it; it simply shows a recovery screen when
the module leaves a report.

## What it does

At boot the module looks for a file named `RESCUE` (or `RESCUE.txt`) in
`sd:/switch/playguard/`:

- **present, empty or any text** → it unlocks parental controls temporarily,
  using the PIN the console already stores, exactly as the system PIN screen
  would. Everything can start again, so you can open PlayGuard and fix things
  properly (set a new PIN, adjust the limit, or remove the controls).
- **a line that reads just `delete`** (any case; spaces around it and a
  byte-order mark are ignored) → it deletes every parental control (PIN and all
  restrictions). Irreversible; use it only if you are giving up on the PIN.
  The word inside other text (`delete everything`, `don't delete`) is not
  enough: that file only unlocks.

The request file is **removed before anything is done** (it acts once). Should
the card refuse that, it is renamed `RESCUE.done` instead; should that fail too,
the file is still there, so a delete is **not done** (it would repeat at every
boot) while an unlock still is — and PlayGuard's recovery screen tells you to
remove the file from a computer. The outcome is written to
`rescue_report.txt`. The next time PlayGuard opens it
shows what happened, lets you finish, records it in the change history, and
removes the report. A report it cannot read is removed too, and PlayGuard says
so (check the PIN and the play timer yourself). Should the SD card not be
mounted at boot (the module retries for about 10 s), nothing is done and no
report is left.

## How to use it

1. Install the module once: extract `playguard-rescue.zip` to the SD-card root
   (it lands in `atmosphere/contents/4200000000505247/`). Reboot.
2. When you are locked out: put the SD card in a computer, create an empty file
   `switch/playguard/RESCUE` (or `RESCUE.txt`; to delete everything instead,
   write `delete` on a line of its own), put the card back, turn the
   console on. Open PlayGuard and finish from the recovery screen.
3. Remove `atmosphere/contents/4200000000505247` when you no longer need it:
   while it is installed, anyone who can edit the SD card can use it.

Prefer the empty `RESCUE` file, then *Delete all parental controls* on the
recovery screen, to the `delete` request: PlayGuard saves a backup of the
settings first, while `delete` erases everything at boot with no backup.

## What it is not

- **Not a security hole beyond what already exists.** Anyone who can edit the
  SD card can already turn off PlayGuard's own PIN prompt by editing
  `config.json`, or wipe parental controls with any pctl tool. The module needs
  the same physical access to the card. It changes nothing a determined person
  with the card could not already do; it just makes the honest "I forgot the
  PIN" case recoverable without a computer-side guide.
- **Not for stock consoles.** Like all of PlayGuard it needs Atmosphère.

## Build

```sh
make -C sysmodule          # -> sysmodule/out/playguard-rescue.nsp (needs devkitPro, DEVKITPRO set)
make dist-rescue           # -> playguard-rescue.zip  (SD-card layout, from the repo root)
```

The module shares `source/core/rescue.c` with the app (the request / report
file format, host-tested in `tests/rescue/`). Its own pctl calls are the ones
documented in `source/core/pctl_ops.c`, kept minimal and self-contained so the
sysmodule pulls in nothing from the app's service layer.

Program ID `0x4200000000505247` is in the private range homebrew sysmodules use,
so it never collides with a real title.

## License

GPLv3 (see [`../LICENSE`](../LICENSE)).
