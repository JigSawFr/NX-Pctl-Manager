#!/usr/bin/env python3
"""Static checks on the app resources (run in CI and by `make check`).

- every i18n JSON file parses and en-US / fr define exactly the same keys;
- every "nx_pctl/..." key referenced from C++ or XML exists in en-US
  (keys built at runtime are checked as prefixes: "nx_pctl/days/{}" etc.);
- every XML layout is well formed (borealis' "brls:" prefix is not declared,
  so a non-namespace-aware parser is used);
- brls:Label never carries padding attributes (borealis throws at runtime).
"""
import glob
import json
import re
import sys
import xml.parsers.expat

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
errors = []


def flatten(d, prefix=""):
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(flatten(v, prefix + k + "/"))
        else:
            out[prefix + k] = v
    return out


catalogs = {}
for lang in ("en-US", "fr"):
    path = f"{ROOT}/resources/i18n/{lang}/nx_pctl.json"
    try:
        catalogs[lang] = flatten(json.load(open(path, encoding="utf-8")))
    except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {e}")
for path in glob.glob(f"{ROOT}/resources/i18n/*/*.json"):
    try:
        json.load(open(path, encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {e}")

if len(catalogs) == 2:
    en, fr = set(catalogs["en-US"]), set(catalogs["fr"])
    for k in sorted(en - fr):
        errors.append(f"fr is missing {k}")
    for k in sorted(fr - en):
        errors.append(f"fr has extra key {k}")
    # The same number of "{}" placeholders in both languages.
    for k in sorted(en & fr):
        if catalogs["en-US"][k].count("{}") != catalogs["fr"][k].count("{}"):
            errors.append(f"placeholder count differs for {k}")

en_keys = set(catalogs.get("en-US", {}))
prefixes = {k.rsplit("/", 1)[0] + "/" for k in en_keys}


def check_key(key, where):
    if "{" in key or key.endswith("/"):
        prefix = key.split("{")[0]
        if not any(p.startswith(prefix.replace("nx_pctl/", "", 1)) for p in prefixes):
            errors.append(f"{where}: no key under prefix {key}")
        return
    if key.replace("nx_pctl/", "", 1) not in en_keys:
        errors.append(f"{where}: unknown i18n key {key}")


for path in glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r'"(nx_pctl/[A-Za-z0-9_/{}.-]*)"', text):
        check_key(m.group(1), path)

for path in sorted(glob.glob(f"{ROOT}/resources/xml/**/*.xml", recursive=True)):
    data = open(path, "rb").read()
    parser = xml.parsers.expat.ParserCreate()

    def start(name, attrs, path=path):
        if name == "brls:Label" and any(a.startswith("padding") for a in attrs):
            errors.append(f"{path}: brls:Label does not support padding attributes")
        for value in attrs.values():
            if value.startswith("@i18n/nx_pctl/"):
                check_key(value[len("@i18n/"):], path)

    parser.StartElementHandler = start
    try:
        parser.Parse(data, True)
    except xml.parsers.expat.ExpatError as e:
        errors.append(f"{path}: {e}")

for e in errors:
    print("ERROR:", e)
print(f"resources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
