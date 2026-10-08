#!/usr/bin/env python3
"""Static checks on the app resources (run in CI and by `make check`).

- every i18n JSON file parses, and every language (resources/i18n/<code>/
  playguard.json) defines exactly the keys of en-US, with as many "{}";
- config::LANGUAGES (source/util/config.hpp) lists exactly those languages,
  and each has a name under playguard/tools/languages/;
- every "playguard/..." key referenced from C++ or XML exists in en-US
  (keys built at runtime are checked as prefixes: "playguard/days/{}" etc.);
- every en-US key is referenced (exactly, or under such a prefix): no dead
  strings for translators to keep up to date;
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


BASE = "en-US"
catalogs = {}
for path in sorted(glob.glob(f"{ROOT}/resources/i18n/*/playguard.json")):
    lang = path.split("/")[-2]
    try:
        catalogs[lang] = flatten(json.load(open(path, encoding="utf-8")))
    except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {e}")
for path in glob.glob(f"{ROOT}/resources/i18n/*/*.json"):
    if not path.endswith("/playguard.json"):
        try:
            json.load(open(path, encoding="utf-8"))
        except Exception as e:  # noqa: BLE001
            errors.append(f"{path}: {e}")
if BASE not in catalogs:
    errors.append(f"resources/i18n/{BASE}/playguard.json is missing")

base = catalogs.get(BASE, {})
for lang, cat in sorted(catalogs.items()):
    if lang == BASE:
        continue
    for k in sorted(set(base) - set(cat)):
        errors.append(f"{lang} is missing {k}")
    for k in sorted(set(cat) - set(base)):
        errors.append(f"{lang} has extra key {k}")
    # The same number of "{}" placeholders as en-US.
    for k in sorted(set(base) & set(cat)):
        if str(base[k]).count("{}") != str(cat[k]).count("{}"):
            errors.append(f"{lang}: placeholder count differs for {k}")

# The language picker's list (config::LANGUAGES) and the catalogs agree.
config_hpp = open(f"{ROOT}/source/util/config.hpp", encoding="utf-8").read()
m = re.search(r"LANGUAGES\[\]\s*=\s*\{([^}]*)\}", config_hpp)
if not m:
    errors.append("source/util/config.hpp: config::LANGUAGES not found")
else:
    listed = re.findall(r'"([^"]+)"', m.group(1))
    if not listed or listed[0] != "system":
        errors.append('config::LANGUAGES must start with "system"')
    for lang in sorted(set(listed) - {"system"} - set(catalogs)):
        errors.append(f"config::LANGUAGES lists {lang}, which has no resources/i18n/{lang}/playguard.json")
    for lang in sorted(set(catalogs) - set(listed)):
        errors.append(f"resources/i18n/{lang} is not in config::LANGUAGES (the language picker)")
    for lang in listed:
        if f"tools/languages/{lang}" not in base:
            errors.append(f"no language name playguard/tools/languages/{lang}")

en_keys = set(base)
prefixes = {k.rsplit("/", 1)[0] + "/" for k in en_keys}


used_exact = set()
used_prefixes = set()


def check_key(key, where):
    if "{" in key or key.endswith("/"):
        prefix = key.split("{")[0]
        used_prefixes.add(prefix.replace("playguard/", "", 1))
        if not any(p.startswith(prefix.replace("playguard/", "", 1)) for p in prefixes):
            errors.append(f"{where}: no key under prefix {key}")
        return
    used_exact.add(key.replace("playguard/", "", 1))
    if key.replace("playguard/", "", 1) not in en_keys:
        errors.append(f"{where}: unknown i18n key {key}")


for path in glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True) + glob.glob(f"{ROOT}/source/**/*.[ch]", recursive=True):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r'"(playguard/[A-Za-z0-9_/{}.-]*)"', text):
        check_key(m.group(1), path)

for path in sorted(glob.glob(f"{ROOT}/resources/xml/**/*.xml", recursive=True)):
    data = open(path, "rb").read()
    parser = xml.parsers.expat.ParserCreate()

    def start(name, attrs, path=path):
        if name == "brls:Label" and any(a.startswith("padding") for a in attrs):
            errors.append(f"{path}: brls:Label does not support padding attributes")
        for value in attrs.values():
            if value.startswith("@i18n/playguard/"):
                check_key(value[len("@i18n/"):], path)

    parser.StartElementHandler = start
    try:
        parser.Parse(data, True)
    except xml.parsers.expat.ExpatError as e:
        errors.append(f"{path}: {e}")

for k in sorted(en_keys):
    if k not in used_exact and not any(k.startswith(p) for p in used_prefixes):
        errors.append(f"unused i18n key playguard/{k} (remove it from every language)")

for e in errors:
    print("ERROR:", e)
print(f"resources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
