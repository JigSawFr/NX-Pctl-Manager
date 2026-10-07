#!/usr/bin/env python3
"""Headless UI smoke test of the desktop build (simulated backend).

Starts build-desktop/playguard on an X display (Xvfb), opens every tab, the
extra-time picker, the per-day editor, a dropdown, the settings backup, a game
in the Activity tab and its PDF export, and fails if the app dies on the way
(borealis throws on unknown XML attributes, missing views, …) or the export is
missing. Screenshots of each screen are written to the output folder.

The "gate" scenario starts on a firmware newer than the checked one, with a
simulated release that supports it: the firmware screen, read-only mode,
seven presses on Version for the developer mode, its report and the firmware
screen again.

Usage: tools/desktop_smoke.py <out-dir> [gate]   (needs DISPLAY, xdotool, ImageMagick)
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
GATE = len(sys.argv) > 2 and sys.argv[2] == "gate"
os.makedirs(OUT, exist_ok=True)
run_dir = os.path.join(OUT, "run")
os.makedirs(run_dir, exist_ok=True)
config_file = os.path.join(run_dir, "playguard_data", "config.json")
if GATE and os.path.exists(config_file):
    os.remove(config_file)   # no remembered choice, developer mode off

env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1")
if GATE:
    env.setdefault("PLAYGUARD_SIM_FW", "24.0.0")
    env.setdefault("PLAYGUARD_SIM_LATEST", "1.1.0:24.0.0")
log = open(os.path.join(OUT, "app.log"), "w")
cmd = ["dbus-run-session", "--", APP] if subprocess.run(["which", "dbus-run-session"], capture_output=True).returncode == 0 else [APP]
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


def finish():
    proc.terminate()
    try:
        proc.wait(5)
    except subprocess.TimeoutExpired:
        proc.kill()
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
    key("Down", 40)            # to the last cell (Source code)
    key("Up", 5)               # Licence, Data folder, Launched as, Check for updates, Version
    key("Return", 7)
    shot("04_dev_enabled")
    # A short press does not get past the bottom edge of a long tab (borealis'
    # natural scrolling needs the button held for a few frames): press like a hand.
    key("Down", 12, hold=0.15) # the Developer section, down to its last cell
    shot("05_dev_tools")
    key("Up", 2)               # Show the diagnostic report
    key("Return")
    shot("06_dev_report")
    key("Escape")
    key("Down")                # Show the firmware screen again
    key("Return")
    shot("07_gate_again")
    key("Escape")              # B: continue read-only
    shot("08_back")
    try:
        cfg = json.load(open(config_file))
    except (OSError, ValueError) as e:
        fail(f"no config saved: {e}")
    if cfg.get("dev_mode") is not True:
        fail("developer mode was not saved")
    if cfg.get("fw_gate_choice"):
        fail("a firmware choice was remembered without the box ticked")
    finish()
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
key("Escape")
key("Escape")
shot("23_back")

# Settings backup: save one, open the list and the restore summary (cancelled).
key("Left")                # back to the sidebar
key("Down", len(tabs) - 2) # Tools & about
key("Right")
key("Down")                # Back up the settings
key("Return")
shot("24_backup_saved")
key("Down")                # Restore a backup…
key("Return")
shot("25_backup_list")
key("Return")
shot("26_backup_restore")
key("Escape")
shot("27_back")

# Activity: one game's details, then a PDF export to the (simulated) SD card.
key("Left")                # back to the sidebar
key("Up", len(tabs) - 3)   # Activity
key("Right")
key("Down", 4)             # past Today, Last 7 days, Sort by and Export: the first game
key("Return")
shot("28_activity_game")
key("Escape")
key("Up")                  # Export to the SD card…
key("Return")
key("Down", 3)             # PDF
key("Return")
shot("29_activity_export")
exports = os.path.join(run_dir, "playguard_data", "exports")
if not any(f.endswith(".pdf") for f in (os.listdir(exports) if os.path.isdir(exports) else [])):
    fail("no PDF export in " + exports)

finish()
