#!/usr/bin/env python3
"""Visual regression check of the desktop smoke test's screenshots.

Compares every reference image of <reference-dir> (tests/visual/<scenario>/
NAME.png) with NAME.png in <shots-dir>, the output folder of
tools/desktop_smoke.py, and fails when a screen changed. The smoke test fixes
the console's time (PLAYGUARD_SIM_NOW, TZ), so only the footer clock follows
the host (borealis reads it): it is redrawn at the console's time, 16:00:00,
in the clock's own font, colours and place, in the references as in the
screenshots compared with them. A pixel counts as changed when its colour moves by
more than --fuzz (15 %: the focus highlight's animated glow stays below that);
a screen fails when more than --max-pixels pixels change (100: two runs differ
by a few dozen at most, a value going from "2 h" to "2 h 1" by about 200). For
each failing screen, <shots-dir>/visual_diff/NAME.png shows the reference, the
new screenshot and the changes (in red) side by side.

Only screens that come out the same from one run to the next are kept as
references: no toast, no held-key scrolling, no app version (a release would
change it), no SD-card content that piles up between local runs.

A change to the UI that is meant: run the smoke test, then
  tools/visual_check.py <shots-dir> <reference-dir> --update
which copies the new screenshots over the references (--add NAME ... adds
screens to the set), and look at the images before committing them.

Usage: tools/visual_check.py <shots-dir> <reference-dir> [--update] [--add NAME ...]
                             [--fuzz PERCENT] [--max-pixels N]     (needs ImageMagick)
"""
import argparse
import collections
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

# The footer clock (bottom left): the host's time, different in every run.
# It is redrawn as the console's time in the smoke test (PLAYGUARD_SIM_NOW
# 1791475200, TZ=UTC), in borealis' footer font, where the label is drawn.
CLOCK_AREA = (30, 655, 220, 50)         # x, y, width, height: the clock only (a dialog starts at x 280)
CLOCK_TEXT = "16:00:00"
CLOCK_ORIGIN = (55, 692)                # the label's text origin (baseline)
CLOCK_SIZE = "21.5"                     # brls/hints/time fontSize
CLOCK_FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                          "extern", "borealis", "resources", "font", "switch_font.ttf")
INK = 60   # a pixel this far (sum of the RGB differences) from the background is text


def area_pixels(src):
    """The clock area's pixels, {(x, y): (r, g, b)}, in area coordinates."""
    x, y, w, h = CLOCK_AREA
    out = subprocess.run(["convert", src, "-crop", f"{w}x{h}+{x}+{y}", "+repage", "-depth", "8", "txt:-"],
                         capture_output=True, text=True, check=True).stdout
    px = {}
    for line in out.splitlines()[1:]:   # "12,3: (235,235,235)  #EBEBEB  srgb(...)"
        m = re.match(r"(\d+),(\d+): \((\d+),(\d+),(\d+)", line)
        if m:
            px[(int(m[1]), int(m[2]))] = (int(m[3]), int(m[4]), int(m[5]))
    return px


def masked(src, dst):
    """`src` with the footer clock showing CLOCK_TEXT: the area filled with
    its background, then the text in the clock's colour (both read from
    `src`, so a footer dimmed under a dialog stays dimmed). No clock there:
    only the background."""
    px = area_pixels(src)
    bg = collections.Counter(px.values()).most_common(1)[0][0]
    distance = lambda c: sum(abs(a - b) for a, b in zip(c, bg))
    ink = [c for c in px.values() if distance(c) > INK]
    x, y, w, h = CLOCK_AREA
    hex_colour = lambda c: "#%02x%02x%02x" % c
    cmd = ["convert", src, "-fill", hex_colour(bg), "-draw", f"rectangle {x},{y} {x + w - 1},{y + h - 1}"]
    if ink:
        # The pixels fully inside a stroke: the text colour.
        far = max(distance(c) for c in ink)
        fg = collections.Counter(c for c in ink if distance(c) >= 0.9 * far).most_common(1)[0][0]
        cmd += ["-font", CLOCK_FONT, "-pointsize", CLOCK_SIZE, "-fill", hex_colour(fg),
                "-annotate", f"+{CLOCK_ORIGIN[0]}+{CLOCK_ORIGIN[1]}", CLOCK_TEXT]
    subprocess.run(cmd + [dst], check=True)


def size(path):
    out = subprocess.run(["identify", "-format", "%wx%h", path], capture_output=True, text=True, check=True)
    return out.stdout.strip()


def changed_pixels(ref, new, fuzz, diff):
    """Pixels that differ by more than `fuzz` %, with the changes drawn in `diff`."""
    out = subprocess.run(["compare", "-metric", "AE", "-fuzz", f"{fuzz}%", ref, new, diff],
                         capture_output=True, text=True)
    if out.returncode not in (0, 1):   # 2: compare itself failed
        raise RuntimeError(out.stderr.strip())
    # "1234", or "1234 (0.0188)" in newer ImageMagick versions.
    return int(float(out.stderr.split()[0]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("shots")
    ap.add_argument("reference")
    ap.add_argument("--update", action="store_true", help="copy the screenshots over the references")
    ap.add_argument("--add", nargs="*", default=[], metavar="NAME", help="with --update: screens to add")
    ap.add_argument("--fuzz", type=float, default=15.0)
    ap.add_argument("--max-pixels", type=int, default=100)
    args = ap.parse_args()

    names = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(args.reference, "*.png")))
    if args.update:
        os.makedirs(args.reference, exist_ok=True)
        for name in sorted(set(names) | set(args.add)):
            src = os.path.join(args.shots, name + ".png")
            if not os.path.exists(src):
                sys.exit(f"no {src}")
            # Stored with the footer clock at CLOCK_TEXT, never the host's time.
            masked(src, os.path.join(args.reference, name + ".png"))
            subprocess.run(["mogrify", "-strip", os.path.join(args.reference, name + ".png")], check=True)
            print("updated", name)
        return 0
    if not names:
        sys.exit(f"no reference image in {args.reference}")

    failed = []
    diff_dir = os.path.join(args.shots, "visual_diff")
    with tempfile.TemporaryDirectory() as tmp:
        for name in names:
            ref = os.path.join(args.reference, name + ".png")
            new = os.path.join(args.shots, name + ".png")
            if not os.path.exists(new):
                print(f"FAIL {name}: no screenshot {new}")
                failed.append(name)
                continue
            if size(ref) != size(new):
                print(f"FAIL {name}: {size(new)} instead of {size(ref)}")
                failed.append(name)
                continue
            ref_m, new_m = os.path.join(tmp, "ref.png"), os.path.join(tmp, "new.png")
            masked(ref, ref_m)
            masked(new, new_m)
            diff = os.path.join(tmp, "diff.png")
            n = changed_pixels(ref_m, new_m, args.fuzz, diff)
            if n <= args.max_pixels:
                print(f"ok   {name} ({n} px)")
                continue
            print(f"FAIL {name}: {n} pixels changed (at most {args.max_pixels})")
            failed.append(name)
            os.makedirs(diff_dir, exist_ok=True)
            subprocess.run(["convert", ref_m, new_m, diff, "+append", os.path.join(diff_dir, name + ".png")], check=True)

    if failed:
        print(f"\nvisual check: {len(failed)} screen(s) changed: {', '.join(failed)}")
        print(f"Side-by-side images (reference | now | changes): {diff_dir}")
        print("If the change is meant: tools/visual_check.py <shots-dir> <reference-dir> --update")
        return 1
    print(f"visual check: {len(names)} screen(s) unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
