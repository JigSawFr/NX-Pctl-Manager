#!/usr/bin/env python3
"""Headless UI smoke test of the desktop build (simulated backend).

Starts build-desktop/playguard on an X display (Xvfb), opens every tab, the
extra-time picker, the per-day editor, a dropdown, the settings backup, a game
in the Activity tab and its PDF export, and fails if the app dies on the way
(borealis throws on unknown XML attributes, missing views, …) or the export is
missing. Screenshots of each screen are written to the output folder.

The "gate" scenario starts on a firmware newer than the checked one, with a
simulated release that supports it: the firmware screen, read-only mode,
seven presses on Version for the developer mode, its report, a play-timer
block reference, the firmware screen again and the hand-over of the update
to sphaira (simulated hbloader).

The "errors" scenario starts with today's limit reached, the temporary unlock
failing and "Synchronise clock via Internet" off (PLAYGUARD_SIM_FAIL & co.):
a limit change must end in the "could not unlock" toast, with the app alive,
and the clock tab must say why the network clock cannot be set.

Usage: tools/desktop_smoke.py <out-dir> [gate|errors]   (needs DISPLAY, xdotool, ImageMagick)
Environment knobs of the simulated backend (PLAYGUARD_SIM_*) are passed through.
"""
import json
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build-desktop", "playguard")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "smoke")
SCENARIO = sys.argv[2] if len(sys.argv) > 2 else ""
GATE = SCENARIO == "gate"
ERRORS = SCENARIO == "errors"
os.makedirs(OUT, exist_ok=True)
run_dir = os.path.join(OUT, "run")
os.makedirs(run_dir, exist_ok=True)
config_file = os.path.join(run_dir, "playguard_data", "config.json")
if (GATE or ERRORS) and os.path.exists(config_file):
    os.remove(config_file)   # no remembered choice, developer mode off
block_ref = os.path.join(run_dir, "playguard_data", "logs", "play_timer_block.json")
if GATE and os.path.exists(block_ref):
    os.remove(block_ref)     # the developer tool must write a new one

env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1")
if GATE:
    env.setdefault("PLAYGUARD_SIM_FW", "24.0.0")
    env.setdefault("PLAYGUARD_SIM_LATEST", "1.1.0:24.0.0")
    # Started through hbloader, with sphaira on the SD card: the update can be
    # handed over to it.
    env.setdefault("PLAYGUARD_SIM_HBLOADER", "1")
    sphaira = os.path.join(run_dir, "playguard_data", "sd", "switch", "sphaira", "sphaira.nro")
    os.makedirs(os.path.dirname(sphaira), exist_ok=True)
    open(sphaira, "w").write("NRO0")
if not GATE and not ERRORS:
    env.setdefault("PLAYGUARD_SIM_NUMPAD", "1:30")   # what the system number pad returns
if ERRORS:
    env.setdefault("PLAYGUARD_SIM_FAIL", "unlock")
    env.setdefault("PLAYGUARD_SIM_RESTRICTED", "1")
    env.setdefault("PLAYGUARD_SIM_AUTOSYNC_OFF", "1")
log = open(os.path.join(OUT, "app.log"), "w")
def have(tool):
    return subprocess.run(["which", tool], capture_output=True).returncode == 0


# Line-buffered output, so app.log is complete at any moment (toasts()).
cmd = (["stdbuf", "-oL", "-eL"] if have("stdbuf") else []) + [APP]
if have("dbus-run-session"):
    cmd = ["dbus-run-session", "--"] + cmd
proc = subprocess.Popen(cmd, cwd=run_dir, env=env, stdout=log, stderr=subprocess.STDOUT)


def alive():
    return proc.poll() is None


def key(name, n=1, hold=0.02):
    """Presses `name` n times, each exactly once. While Down / Up is held,
    borealis' ScrollingFrame keeps scrolling ("natural scrolling") and can move
    the focus again; with frames as slow as under software GL, even an 80 ms
    press did. So each press waits for the screen to settle (the app then waits
    for input and sees the press at once) and is released `hold` seconds later
    (20 ms by default), both
    sent by one xdotool process so a busy runner cannot delay the release."""
    for _ in range(n):
        time.sleep(0.6)
        subprocess.run(["xdotool", "keydown", name, "sleep", str(hold), "keyup", name], env=env)
        time.sleep(0.2)


def shot(name):
    time.sleep(0.8)
    subprocess.run(["import", "-window", "root", os.path.join(OUT, name + ".png")], env=env)
    if not alive():
        fail(f"app exited while showing {name}")


def toasts():
    """What the app told the user so far (ui::notify logs every toast)."""
    log.flush()
    return [l.split("toast: ", 1)[1].strip() for l in open(os.path.join(OUT, "app.log"), errors="replace") if "toast: " in l]


def fail(msg):
    log.flush()
    print("SMOKE FAILED:", msg)
    print(open(os.path.join(OUT, "app.log")).read()[-3000:])
    sys.exit(1)


for _ in range(60):
    if subprocess.run(["xdotool", "search", "--name", "PlayGuard"], env=env, capture_output=True).returncode == 0:
        break
    if not alive():
        fail("app exited during start-up")
    time.sleep(0.5)
else:
    fail("no window after 30 s")
time.sleep(2)

tabs = ["dashboard", "play_timer", "activity", "restrictions", "clock", "security", "tools"]


def finish(check=None):
    """Stops the app, then runs `check` (on its complete log)."""
    proc.terminate()
    try:
        proc.wait(5)
    except subprocess.TimeoutExpired:
        proc.kill()
    if check:
        check()
    print("desktop smoke test passed:", OUT)
    sys.exit(0)


if GATE:
    shot("01_gate")            # a release supports the firmware: its cell has the focus
    key("Down")                # Continue in read-only mode
    key("Return")
    shot("02_read_only")       # title "PlayGuard · read-only"
    key("Down")                # Play timer: the read-only note, no limit editors
    key("Right")
    shot("03_play_timer_read_only")
    key("Left")
    # Tools: seven presses on Version turn the developer mode on.
    key("Down", len(tabs) - 2)
    key("Right")
    key("Down", 40, hold=0.15) # to the last cell (Source code), held: a long tab
    key("Up", 4)               # Licence, Data folder, Launched as, Version
    key("Return", 7)
    shot("04_dev_enabled")
    # A short press does not get past the bottom edge of a long tab (borealis'
    # natural scrolling needs the button held for a few frames): press like a hand.
    key("Down", 14, hold=0.15) # the Developer section, down to its last cell
    shot("05_dev_tools")
    key("Up", 3)               # Show the diagnostic report
    key("Return")
    shot("06_dev_report")
    key("Escape")
    key("Down")                # Compare the play-timer block: none saved yet
    key("Return")
    shot("06_dev_block")
    key("Right")               # Save the reference (read-only mode only writes to the SD card)
    key("Return")
    if not os.path.exists(block_ref):
        fail("no play-timer block reference in " + block_ref)
    key("Down")                # Show the firmware screen again
    key("Return")
    shot("07_gate_again")
    key("Return")              # How to update (it has the focus): open sphaira?
    shot("08_open_store")
    key("Right")               # Open sphaira: PlayGuard hands over to hbloader and quits
    key("Return")
    for _ in range(20):
        if not alive():
            break
        time.sleep(0.5)
    else:
        fail("still running after handing over to sphaira")
    log.flush()
    if "next load: sdmc:/switch/sphaira/sphaira.nro" not in open(os.path.join(OUT, "app.log"), errors="replace").read():
        fail("sphaira was not set as the next homebrew")
    try:
        cfg = json.load(open(config_file))
    except (OSError, ValueError) as e:
        fail(f"no config saved: {e}")
    if cfg.get("dev_mode") is not True:
        fail("developer mode was not saved")
    if cfg.get("fw_gate_choice"):
        fail("a firmware choice was remembered without the box ticked")
    finish()
if ERRORS:
    shot("01_limit_reached")   # Overview: today's limit reached
    key("Down")                # Play timer
    key("Right")               # Same limit every day
    key("Return")
    shot("02_picker")          # the days differ: "Custom…" is selected
    key("Up")                  # No play (0 min)
    key("Return")
    shot("03_gate")            # the change needs the temporary unlock
    key("Right")               # Unlock and apply
    key("Return")
    shot("04_unlock_failed")
    key("Left")                # back to the sidebar
    key("Down", 3)             # Network clock
    key("Right")
    shot("05_clock_autosync_off")

    def told_unlock_failed():
        if not any(t.startswith("Could not unlock parental controls") for t in toasts()):
            fail("no 'could not unlock' toast; toasts: " + repr(toasts()))
    finish(told_unlock_failed)

shot("01_dashboard")
# Left (not Escape) goes back to the sidebar: Escape there asks to quit, so a
# step that went wrong could close the app.
for i, tab in enumerate(tabs[1:], start=2):
    key("Down")
    key("Right")
    shot(f"{i:02d}_{tab}")
    key("Down", 12)   # scroll through the whole tab
    shot(f"{i:02d}_{tab}_end")
    key("Left")       # back to the sidebar

# Extra-time picker, per-day editor and a dropdown.
key("Up", len(tabs) - 2)   # from Tools back to Play timer
key("Right")
key("Down")                # Extra time today…
key("Return")
shot("20_extra_time")
key("Escape")
key("Down")                # A different limit for each day…
key("Return")
shot("21_per_day")
key("Return")
shot("22_dropdown")
key("Down", 15)            # to the end of the list (No limit) …
key("Up")                  # … then Enter minutes…: the number pad types "1:30"
key("Return")
shot("22_numpad")          # the first day: 1 h 30 (not saved)
if not any("numpad: " in l and l.rstrip().endswith("-> 1:30") for l in open(os.path.join(OUT, "app.log"), errors="replace")):
    fail("the number pad was not asked for the minutes")
key("Escape")              # unsaved: asks before leaving
shot("23_discard")
key("Right")               # Discard
key("Return")
shot("23_back")

# Settings backup: save one, open the list and the restore summary (cancelled).
key("Left")                # back to the sidebar
key("Down", len(tabs) - 2) # Tools & about
key("Right")
backups = os.path.join(run_dir, "playguard_data", "backups")
before = len(os.listdir(backups)) if os.path.isdir(backups) else 0
key("Return")              # Back up the settings (the first cell)
shot("24_backup_saved")
if (len(os.listdir(backups)) if os.path.isdir(backups) else 0) != before + 1:
    fail("Return on 'Back up the settings' saved no backup (wrong cell focused?)")
key("Down")                # Restore a backup…
key("Return")
shot("25_backup_list")
key("Return")
shot("26_backup_restore")
key("Escape")
shot("27_back")

# Activity: one game's details, then a PDF export to the (simulated) SD card.
exports = os.path.join(run_dir, "playguard_data", "exports")
def pdfs():
    return {f for f in (os.listdir(exports) if os.path.isdir(exports) else []) if f.endswith(".pdf")}
pdfs_before = pdfs()   # the run folder is kept between local runs
key("Left")                # back to the sidebar
key("Up", len(tabs) - 3)   # Activity
key("Right")
key("Down", 5)             # past Today, Last 7 days, All time, Period and Export: the first game
key("Return")
shot("28_activity_game")
key("Escape")
key("Up")                  # Export to the SD card…
key("Return")
key("Down", 3)             # PDF
key("Return")
shot("29_activity_export")
if not pdfs() - pdfs_before:
    fail("no new PDF export in " + exports)

finish()
