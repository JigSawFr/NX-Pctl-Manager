#!/usr/bin/env python3
"""Headless UI smoke test of the desktop build (simulated backend).

Starts build-desktop/playguard on an X display (Xvfb), opens every tab, the
extra-time picker, the per-day editor (the week chart's day picker and the
number pad), a bedtime alarm change, the settings backup, a game's screen in the Activity tab and its
PDF export, and fails if the app dies on the way (borealis throws on unknown
XML attributes, missing views, …) or the export is missing. Screenshots of
each screen are written to the output folder.

The "gate" scenario starts on a firmware newer than the checked one, with a
simulated release that supports it: the firmware screen, read-only mode,
seven presses on About › Version for the developer mode, its report, a play-timer
block reference, the firmware screen again and the hand-over of the update
to sphaira (simulated hbloader).

The "devbuild" scenario turns the developer mode on, signs in to a simulated
GitHub (PLAYGUARD_SIM_GITHUB_LOGIN) from the build list, then installs a pull
request's build out of its artifact zip (PLAYGUARD_SIM_DEV_BUILDS): the file
must replace the simulated playguard.nro and be handed to hbloader. On the
way, the list must be kept (opened again at once) and Refresh fetch it again.

The "errors" scenario starts with today's limit reached, the temporary unlock
failing and "Synchronise clock via Internet" off (PLAYGUARD_SIM_FAIL & co.):
a limit change must end in the "could not unlock" dialog, with the app alive,
and the clock tab must say why the network clock cannot be set.

Usage: tools/desktop_smoke.py <out-dir> [gate|errors|rescue|forged|lock|devbuild]   (needs DISPLAY, xdotool, ImageMagick)
Environment knobs of the simulated backend (PLAYGUARD_SIM_*) are passed through;
the console time is fixed (PLAYGUARD_SIM_NOW, TZ) unless set.
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
RESCUE = SCENARIO == "rescue"
FORGED = SCENARIO == "forged"
LOCK = SCENARIO == "lock"
DEVBUILD = SCENARIO == "devbuild"
os.makedirs(OUT, exist_ok=True)
run_dir = os.path.join(OUT, "run")
os.makedirs(run_dir, exist_ok=True)
config_file = os.path.join(run_dir, "playguard_data", "config.json")
if (GATE or ERRORS or DEVBUILD) and os.path.exists(config_file):
    os.remove(config_file)   # no remembered choice, developer mode off
block_ref = os.path.join(run_dir, "playguard_data", "logs", "play_timer_block.json")
if GATE and os.path.exists(block_ref):
    os.remove(block_ref)     # the developer tool must write a new one

env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1")
# One console time and time zone for every run, so the screenshots can be
# compared with the reference ones (tools/visual_check.py): Thursday
# 8 October 2026, 16:00 UTC. Only the footer clock follows the host.
env.setdefault("PLAYGUARD_SIM_NOW", "1791475200")
env.setdefault("TZ", "UTC")
if GATE:
    env.setdefault("PLAYGUARD_SIM_FW", "24.0.0")
    env.setdefault("PLAYGUARD_SIM_LATEST", "1.1.0:24.0.0")
    # Started through hbloader, with sphaira on the SD card: the update can be
    # handed over to it.
    env.setdefault("PLAYGUARD_SIM_HBLOADER", "1")
    sphaira = os.path.join(run_dir, "playguard_data", "sd", "switch", "sphaira", "sphaira.nro")
    os.makedirs(os.path.dirname(sphaira), exist_ok=True)
    open(sphaira, "w").write("NRO0")
if DEVBUILD:
    # Developer tools › Install another build, against a simulated GitHub: the
    # latest release, then (once signed in, simulated device flow) two commits
    # of main and an open pull request whose artifact is a local zip holding
    # playguard.nro.
    import hashlib
    import zipfile
    env.setdefault("PLAYGUARD_SIM_HBLOADER", "1")
    env.setdefault("PLAYGUARD_SIM_GITHUB_LOGIN", "ok")
    sim = os.path.join(run_dir, "sim_github")
    os.makedirs(sim, exist_ok=True)
    token_file = os.path.join(run_dir, "playguard_data", "github_token")
    if os.path.exists(token_file):
        os.remove(token_file)   # starts signed out
    kept_list = os.path.join(run_dir, "playguard_data", "cache", "dev_builds.json")
    if os.path.exists(kept_list):
        os.remove(kept_list)    # and with no list kept from an earlier run
    pr_content = b"\0" * 16 + b"NRO0" + b"pull request build" * 100
    pr_zip = os.path.join(sim, "pr.zip")
    with zipfile.ZipFile(pr_zip, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("playguard.nro", pr_content)
        z.writestr("build-info.txt", "commit: ccccccc\n")
    zip_bytes = open(pr_zip, "rb").read()
    def artifact(branch, sha, date, path, size, digest=None, head_repo=1):
        a = {"name": "playguard_release", "size_in_bytes": size, "expired": False, "created_at": date,
             "archive_download_url": "https://" + path,
             "workflow_run": {"id": 1, "repository_id": 1, "head_repository_id": head_repo,
                              "head_branch": branch, "head_sha": sha}}
        if digest:
            a["digest"] = "sha256:" + digest
        return a
    json.dump({"tag_name": "v1.0.0", "prerelease": False, "draft": False,
               "assets": [{"name": "playguard.nro", "size": 10, "updated_at": "2026-10-01T00:00:00Z",
                           "browser_download_url": "https:///missing"}]},
              open(os.path.join(sim, "latest.json"), "w"))
    json.dump({"artifacts": [
        artifact("main", "a" * 40, "2026-10-09T12:00:00Z", "/missing", 10),
        artifact("main", "b" * 40, "2026-10-08T12:00:00Z", "/missing", 10),
        artifact("feat/report", "c" * 40, "2026-10-09T09:00:00Z", pr_zip, len(zip_bytes),
                 hashlib.sha256(zip_bytes).hexdigest()),
    ]}, open(os.path.join(sim, "artifacts.json"), "w"))
    json.dump([{"number": 38, "title": "feat: send a report online",
                "head": {"ref": "feat/report", "repo": {"id": 1}}}],
              open(os.path.join(sim, "pulls.json"), "w"))
    env.setdefault("PLAYGUARD_SIM_DEV_BUILDS", sim)
    installed_nro = os.path.join(run_dir, "playguard_data", "sd", "switch", "playguard", "playguard.nro")
    os.makedirs(os.path.dirname(installed_nro), exist_ok=True)
    open(installed_nro, "wb").write(b"the build before")
if not GATE and not ERRORS and not DEVBUILD:
    env.setdefault("PLAYGUARD_SIM_NUMPAD", "1:30")   # what the system number pad returns
    env.setdefault("PLAYGUARD_SIM_PASTE", "https://bpa.st/SMOKE")   # what bpa.st (or GitHub) answers
    github_token = os.path.join(run_dir, "playguard_data", "github_token")
    if os.path.exists(github_token):
        os.remove(github_token)   # signed out: the first report goes to bpa.st
if ERRORS:
    env.setdefault("PLAYGUARD_SIM_FAIL", "unlock")
    env.setdefault("PLAYGUARD_SIM_RESTRICTED", "1")
    env.setdefault("PLAYGUARD_SIM_AUTOSYNC_OFF", "1")
if RESCUE:
    # The playguard-rescue sysmodule left a report: PlayGuard must show the
    # recovery screen at start-up and open the app when it is dismissed.
    # The console confirms it: the sysmodule is installed and its unlock is
    # still on (a report the console does not confirm only shows, without
    # the actions).
    env.setdefault("PLAYGUARD_SIM_UNLOCKED", "1")
    sysmodule = os.path.join(run_dir, "playguard_data", "sd", "atmosphere", "contents", "4200000000505247", "exefs.nsp")
    os.makedirs(os.path.dirname(sysmodule), exist_ok=True)
    open(sysmodule, "w").write("NSP0")
    report = os.path.join(run_dir, "playguard_data", "rescue_report.txt")
    os.makedirs(os.path.dirname(report), exist_ok=True)
    open(report, "w").write("mode=unlock\nresult=ok\nrc=0x00000000\nunlocks=1\n")
    history = os.path.join(run_dir, "playguard_data", "history.json")
    if os.path.exists(history):
        os.remove(history)   # a clean history: the rescue entry must be the only one
if FORGED:
    # A rescue report written by hand while PlayGuard asks for the PIN to
    # open, on a console a parent left temporarily unlocked but without the
    # sysmodule: the recovery screen shows it with no action, and "Open
    # PlayGuard" goes through the lock screen.
    env.setdefault("PLAYGUARD_SIM_UNLOCKED", "1")
    sysmodule = os.path.join(run_dir, "playguard_data", "sd", "atmosphere", "contents", "4200000000505247")
    if os.path.exists(sysmodule):
        import shutil
        shutil.rmtree(sysmodule)
    report = os.path.join(run_dir, "playguard_data", "rescue_report.txt")
    os.makedirs(os.path.dirname(report), exist_ok=True)
    open(report, "w").write("mode=unlock\nresult=ok\nrc=0x00000000\nunlocks=1\n")
    json.dump({"schema": 1, "pin_lock": "open"}, open(config_file, "w"))
if LOCK:
    # Security › Ask for the PIN › To open PlayGuard: the lock screen comes
    # first, and the right PIN (the simulated PIN screen accepts) opens the
    # app. It is the first screen, which borealis never pops.
    os.makedirs(os.path.dirname(config_file), exist_ok=True)
    json.dump({"schema": 1, "pin_lock": "open"}, open(config_file, "w"))
log = open(os.path.join(OUT, "app.log"), "w")
def have(tool):
    return subprocess.run(["which", tool], capture_output=True).returncode == 0


# Line-buffered output, so app.log is complete at any moment (messages()).
cmd = (["stdbuf", "-oL", "-eL"] if have("stdbuf") else []) + [APP]
if have("dbus-run-session"):
    cmd = ["dbus-run-session", "--"] + cmd
proc = subprocess.Popen(cmd, cwd=run_dir, env=env, stdout=log, stderr=subprocess.STDOUT)


def alive():
    return proc.poll() is None


def key(name, n=1, hold=0.02):
    """Presses `name` n times, each exactly once. A held Down / Up repeats
    (ScrollView scrolls a long text, then moves the focus again); with frames
    as slow as under software GL, even an 80 ms press could. So each press waits for the screen to settle (the app then waits
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


def messages():
    """What the app told the user so far: every toast (ui::notify) and every
    error dialog (ui::error), both logged."""
    log.flush()
    out = []
    for l in open(os.path.join(OUT, "app.log"), errors="replace"):
        for tag in ("toast: ", "error: "):
            if tag in l:
                out.append(l.split(tag, 1)[1].strip())
    return out


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

tabs = ["dashboard", "play_timer", "activity", "restrictions", "clock", "security", "preferences", "tools", "about"]


def steps(a, b):
    """Sidebar presses from tab `a` down to tab `b` (negative: up)."""
    return tabs.index(b) - tabs.index(a)


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
    # About: seven presses on Version (its first cell) turn the developer mode on.
    key("Down", steps("play_timer", "about"))
    key("Right")
    key("Return", 7)
    shot("04_dev_enabled")
    # The developer tools are at the end of Tools: press like a hand.
    key("Left")
    key("Up", steps("tools", "about"))
    key("Right")
    key("Down", 50, hold=0.15) # the Developer section, down to its last cell
    shot("05_dev_tools")
    key("Up", 5)               # past GitHub account, Install another build: Show the diagnostic report
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
if DEVBUILD:
    # About: seven presses on Version turn the developer mode on.
    key("Down", steps("dashboard", "about"))
    key("Right")
    key("Return", 7)
    key("Left")
    key("Up", steps("tools", "about"))
    key("Right")
    key("Down", 50, hold=0.15) # the Developer section, down to its last cell
    key("Return")              # Install another build: signed out, the release and "Sign in"
    shot("01_dev_builds_signed_out")
    key("Down")                # Sign in to GitHub
    key("Return")
    shot("02_github_code")     # the code and its QR code (the simulated GitHub approves at once)
    time.sleep(3)
    if not os.path.exists(token_file):
        fail("no GitHub token saved in " + token_file)
    shot("03_dev_builds")      # the list again: release, main ×2, the pull request, Refresh
    cache_file = os.path.join(run_dir, "playguard_data", "cache", "dev_builds.json")
    if not os.path.exists(cache_file) or len(json.load(open(cache_file))["builds"]) != 4:
        fail("the list was not kept in " + cache_file)
    loading = "Looking for builds..."
    fetched = messages().count(loading)
    key("Escape")              # closed, then opened again: the list kept is shown at once
    key("Return")
    shot("03b_dev_builds_kept")
    if messages().count(loading) != fetched:
        fail("a list fetched a moment ago was fetched again")
    key("Down", 4)             # Refresh the list: fetched again
    key("Return")
    time.sleep(2)
    if messages().count(loading) != fetched + 1:
        fail("Refresh the list did not fetch it again")
    key("Down", 3)             # the pull request
    key("Return")
    shot("04_dev_build_confirm")
    key("Right")               # Install
    key("Return")
    for _ in range(20):
        if not alive():
            break
        time.sleep(0.5)
    else:
        fail("still running after installing the build")
    log.flush()
    if "next load: sdmc:" not in open(os.path.join(OUT, "app.log"), errors="replace").read():
        fail("the new build was not set as the next homebrew")
    if open(installed_nro, "rb").read() != pr_content:
        fail("the pull request's build did not replace " + installed_nro)
    for leftover in (".old", ".new", ".new.zip"):
        if os.path.exists(installed_nro + leftover):
            fail("a %s file was left next to %s" % (leftover, installed_nro))
    finish()
if ERRORS:
    shot("01_limit_reached")   # Overview: today's limit reached
    key("Down")                # Play timer: the state line says the limit is reached
    key("Right")               # the week chart (today)
    key("Down")                # Same limit every day
    key("Return")
    shot("02_picker")          # the days differ: "Custom…" is selected
    key("Up")                  # No play (0 min)
    key("Return")
    shot("03_gate")            # the change needs the temporary unlock
    key("Right")               # Unlock and apply
    key("Return")
    shot("04_unlock_failed")   # a dialog, not a toast
    key("Return")              # OK
    key("Left")                # back to the sidebar
    key("Down", 3)             # Network clock
    key("Right")
    shot("05_clock_autosync_off")

    def told_unlock_failed():
        if not any(t.startswith("Could not unlock parental controls") for t in messages()):
            fail("no 'could not unlock' dialog; messages: " + repr(messages()))
    finish(told_unlock_failed)
def main_opened():
    log.flush()
    return any("main screen opened" in l for l in open(os.path.join(OUT, "app.log"), errors="replace"))


if LOCK:
    shot("01_unlocked")        # the PIN screen answered at once: the Overview
    if not main_opened():
        fail("the right PIN left the lock screen up; messages: " + repr(messages()))
    finish()
if FORGED:
    shot("01_recovery_unconfirmed")   # what the file says, no action offered
    def log_text():
        log.flush()
        return open(os.path.join(OUT, "app.log"), errors="replace").read()
    if "rescue report not confirmed" not in log_text():
        fail("a hand-written rescue report was trusted")
    if "pctl_ask_pin" in log_text():
        fail("the PIN was asked before leaving the recovery screen")
    key("Return")              # Open PlayGuard, the only thing left
    shot("02_opened")
    if "pctl_ask_pin returned" not in log_text():
        fail("an unconfirmed rescue report skipped the lock screen")
    if not main_opened():
        fail("the app did not open after the lock screen")
    finish()
if RESCUE:
    shot("01_recovery")        # the recovery screen, in place of the usual first screen
    key("Down", 3)             # past Show the PIN / Set a new PIN / Delete: Open PlayGuard
    key("Return")              # proceed to the app
    shot("02_opened")          # the Overview: the app opened after the rescue
    if not alive():
        fail("app exited instead of opening after recovery")
    if not main_opened():
        fail("the app did not open after recovery")
    report_path = os.path.join(run_dir, "playguard_data", "rescue_report.txt")
    if os.path.exists(report_path):
        fail("the rescue report was not removed after it was read")

    def recorded():
        history = os.path.join(run_dir, "playguard_data", "history.json")
        try:
            entries = json.load(open(history)).get("entries", [])
        except (OSError, ValueError) as e:
            fail(f"no history written: {e}")
        if not any(e.get("kind") == "rescue" for e in entries):
            fail("recovery was not recorded in the change history")
        if not any(e.get("kind") == "relock" for e in entries):
            fail("\"Lock and quit\" did not lock again")

    # Still unlocked (the sysmodule's unlock): quitting asks to lock again.
    def asked_to_relock():
        log.flush()
        return "asking to lock again" in open(os.path.join(OUT, "app.log"), errors="replace").read()
    for _ in range(4):         # back to the sidebar first, then B twice
        if asked_to_relock():
            break
        key("Escape")
        time.sleep(0.3)
    shot("03_quit_unlocked")
    if not alive() or not asked_to_relock():
        fail("quitting while unlocked did not ask to lock again")
    key("Right")               # Quit, still unlocked | Lock and quit
    key("Return")
    for _ in range(50):
        if not alive():
            break
        time.sleep(0.1)
    if alive():
        fail("\"Lock and quit\" did not quit")
    finish(recorded)

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

# Extra-time picker, per-day editor (the week chart is the editor) and its
# day picker.
key("Up", steps("play_timer", tabs[-1]))   # from the last tab back to Play timer
key("Right")               # the week chart, on today
key("Right")               # tomorrow's day
key("Return")              # its limit picker, from the tab itself
shot("19_day_picker")
key("Escape")
key("Down")                # Same limit every day
key("Down")                # Extra time today…
key("Return")
shot("20_extra_time")
key("Escape")
key("Down", 2)             # past No more play today: A different limit for each day…
key("Return")
shot("21_per_day")
key("Return")              # today's limit (the chart has the focus)
shot("22_dropdown")
key("Down", 15)            # to the end of the list (No limit) …
key("Up")                  # … then Enter minutes…: the number pad types "1:30"
key("Return")
shot("22_numpad")          # today: 1 h 30, drawn as unsaved; "+ Save (1)" in the footer
if not any("numpad: " in l and l.rstrip().endswith("-> 1:30") for l in open(os.path.join(OUT, "app.log"), errors="replace")):
    fail("the number pad was not asked for the minutes")
key("Escape")              # unsaved: asks before leaving
shot("23_discard")
key("Right")               # Discard
key("Return")
shot("23_back")

# Bedtime: the alarm picker (on 21:00), 21:30, confirmed with the unlock the
# running timer needs; the tab must then show it.
key("Down", 3)             # past Remove and Profiles…: Bedtime alarm
key("Return")
shot("23_bedtime_picker")
key("Down", 2)             # 21:30
key("Return")
shot("23_bedtime_confirm")
key("Right")               # Unlock and apply
key("Return")
shot("23_bedtime_done")
if not any(m.startswith("Bedtime alarm changed.") for m in messages()):
    fail("the bedtime alarm was not changed: " + repr(messages()[-3:]))

# Settings backup: save one, open the list and the restore summary (cancelled).
key("Left")                # back to the sidebar
key("Down", steps("play_timer", "tools"))
key("Right")               # First steps…
key("Down", 2)             # past the change history: Back up the settings
backups = os.path.join(run_dir, "playguard_data", "backups")
before = len(os.listdir(backups)) if os.path.isdir(backups) else 0
key("Return")              # Back up the settings
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

# Send a report online: the confirmation, then the link and its QR codes.
uploads = os.path.join(run_dir, "playguard_data", "logs", "uploads.txt")
def upload_count():
    return open(uploads).read().count("https://bpa.st/SMOKE") if os.path.exists(uploads) else 0
uploads_before = upload_count()
logs = os.path.join(run_dir, "playguard_data", "logs")
saved = [f for f in (os.listdir(logs) if os.path.isdir(logs) else []) if f[:8].isdigit() and f.endswith(".txt")]
key("Down", 3)             # past Keep, Export: Send a report online
key("Return")
if saved:                  # reports saved by an earlier local run: this report is the first choice
    key("Return")
shot("27_upload_confirm")
key("Right")               # Send
key("Return")
shot("27_upload_link")
if upload_count() != uploads_before + 1:
    fail("the report link was not recorded in " + uploads)
key("Return")              # OK
# Signed in to GitHub: where to send it comes first, a secret gist by default.
open(github_token, "w").write("ghu_smoke")
key("Return")              # Send a report online (still focused)
if any(f[:8].isdigit() and f.endswith(".txt") for f in os.listdir(logs)):
    key("Return")          # this report, the first choice
shot("27_upload_host")
key("Return")              # Secret gist on GitHub
shot("27_upload_gist_confirm")
key("Right")               # Send
key("Return")
shot("27_upload_gist_link")
if upload_count() != uploads_before + 2:
    fail("the gist link was not recorded in " + uploads)
key("Return")              # OK
os.remove(github_token)

# Activity: one game's screen, then a PDF export to the (simulated) SD card.
exports = os.path.join(run_dir, "playguard_data", "exports")
def pdfs():
    return {f for f in (os.listdir(exports) if os.path.isdir(exports) else []) if f.endswith(".pdf")}
pdfs_before = pdfs()   # the run folder is kept between local runs
key("Left")                # back to the sidebar
key("Up", steps("activity", "tools"))
key("Right")               # Account (the simulated console has two; the totals are not focusable)
key("Down", 3)             # past Period and Export: the first game
key("Return")
shot("28_activity_game")   # its own screen: icon, seven days, figures, accounts
key("Escape")
key("Up")                  # Export to the SD card…
key("Return")
key("Down", 3)             # PDF
key("Return")
shot("29_activity_export")
if not pdfs() - pdfs_before:
    fail("no new PDF export in " + exports)

finish()
