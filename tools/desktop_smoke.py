#!/usr/bin/env python3
"""Headless UI smoke test of the desktop build (simulated backend).

Starts build-desktop/playguard on an X display (Xvfb), opens every tab, the
extra-time picker, the per-day editor, a dropdown, the settings backup and a
game in the Activity tab, and fails if the app dies on the way (borealis throws
on unknown XML attributes, missing views, …). Screenshots of each screen are
written to the output folder.

Usage: tools/desktop_smoke.py <out-dir>   (needs DISPLAY, xdotool, ImageMagick)
Environment knobs of the simulated backend (PLAYGUARD_SIM_*) are passed through.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build-desktop", "playguard")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "smoke")
os.makedirs(OUT, exist_ok=True)
run_dir = os.path.join(OUT, "run")
os.makedirs(run_dir, exist_ok=True)

env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1")
log = open(os.path.join(OUT, "app.log"), "w")
cmd = ["dbus-run-session", "--", APP] if subprocess.run(["which", "dbus-run-session"], capture_output=True).returncode == 0 else [APP]
proc = subprocess.Popen(cmd, cwd=run_dir, env=env, stdout=log, stderr=subprocess.STDOUT)


def alive():
    return proc.poll() is None


def key(name, n=1, hold=0.12):
    for _ in range(n):
        subprocess.run(["xdotool", "keydown", name], env=env)
        time.sleep(hold)
        subprocess.run(["xdotool", "keyup", name], env=env)
        time.sleep(0.3)


def nav(name, n=1):
    """A navigation key that must move exactly one step. borealis repeats a key
    held for 250 ms, and a move that scrolls the tab renders slowly under
    software GL: wait for the screen to settle, then press briefly."""
    for _ in range(n):
        time.sleep(1)
        key(name, hold=0.04)


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
shot("01_dashboard")
for i, tab in enumerate(tabs[1:], start=2):
    key("Down")
    key("Right")
    shot(f"{i:02d}_{tab}")
    key("Down", 12)   # scroll through the whole tab
    shot(f"{i:02d}_{tab}_end")
    key("Escape")     # back to the sidebar

# Extra-time picker, per-day editor and a dropdown.
key("Up", len(tabs) - 2)   # from Tools back to Play timer
key("Right")
nav("Down")                # Extra time today…
key("Return")
shot("20_extra_time")
key("Escape")
nav("Down")                # A different limit for each day…
key("Return")
shot("21_per_day")
key("Return")
shot("22_dropdown")
key("Escape")
key("Escape")
shot("23_back")

# Settings backup: save one, open the list and the restore summary (cancelled).
key("Escape")              # back to the sidebar
nav("Down", len(tabs) - 2) # Tools & about
key("Right")
nav("Down")                # Back up the settings
key("Return")
shot("24_backup_saved")
nav("Down")                # Restore a backup…
key("Return")
shot("25_backup_list")
key("Return")
shot("26_backup_restore")
key("Escape")
shot("27_back")

# Activity: one game's details.
key("Escape")              # back to the sidebar
nav("Up", len(tabs) - 3)   # Activity
key("Right")
nav("Down", 3)             # past Today, Last 7 days and Sort by: the first game
key("Return")
shot("28_activity_game")
key("Escape")
shot("29_back")

proc.terminate()
try:
    proc.wait(5)
except subprocess.TimeoutExpired:
    proc.kill()
print("desktop smoke test passed:", OUT)
