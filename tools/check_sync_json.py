#!/usr/bin/env python3
"""Checks the JSON documents the remote link's host tests wrote (make check):
each one parses, the discovery is the device-based form Home Assistant takes
(every component with a platform, a unique id, a state topic and template
or a command topic), and every value template only reads keys the state
document has, so a Home Assistant template never fails on a missing key."""
import json
import re
import sys
from pathlib import Path


def fail(msg):
    print(f"check_sync_json: {msg}")
    sys.exit(1)


def keys_of(doc, prefix=""):
    """Every dotted path in the state document ("timer.limits.mon")."""
    out = set()
    if isinstance(doc, dict):
        for k, v in doc.items():
            path = f"{prefix}.{k}" if prefix else k
            out.add(path)
            out |= keys_of(v, path)
    return out


def main():
    folder = Path(sys.argv[1] if len(sys.argv) > 1 else "build/host-tests/sync")
    docs = {}
    for name in ("discovery.json", "state.json", "state_empty.json", "activity.json"):
        path = folder / name
        if not path.exists():
            fail(f"{path} is missing: run make test first")
        try:
            docs[name] = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            fail(f"{name}: {e}")

    disc = docs["discovery.json"]
    for key in ("dev", "o", "avty_t", "cmps"):
        if key not in disc:
            fail(f"discovery: no {key}")
    state_keys = keys_of(docs["state.json"])
    if keys_of(docs["state_empty.json"]) != state_keys:
        fail("the state read with nothing available has other keys than a full one")
    uniq = set()
    for oid, c in disc["cmps"].items():
        if not re.fullmatch(r"[a-z0-9_]+", oid):
            fail(f"discovery: bad object id {oid!r}")
        for key in ("p", "name", "uniq_id"):
            if key not in c:
                fail(f"discovery: {oid} has no {key}")
        if c["uniq_id"] in uniq:
            fail(f"discovery: {oid} repeats a unique id")
        uniq.add(c["uniq_id"])
        if c["p"] in ("button",):
            if "cmd_t" not in c:
                fail(f"discovery: button {oid} has no command topic")
            continue
        if "stat_t" not in c:
            fail(f"discovery: {oid} has no state topic")
        tpl = c.get("val_tpl")
        if tpl is None:
            continue
        for path in re.findall(r"value_json\.([a-z0-9_.]+)", tpl):
            if path not in state_keys:
                fail(f"discovery: {oid} reads value_json.{path}, which the state does not have")
    print(f"check_sync_json: {len(disc['cmps'])} components, {len(state_keys)} state keys: OK")


if __name__ == "__main__":
    main()
