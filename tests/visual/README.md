# Reference screenshots

What `tools/desktop_smoke.py` shows on screens that come out the same in every
run (console time fixed, footer clock redrawn at the console's time, 16:00:00), one folder per scenario:
`smoke/`, `errors/`, `gate/`, `rescue/`, `forged/`, `lock/`. CI compares each new run with them
(`tools/visual_check.py`) and uploads the side-by-side images of any screen
that changed. A changed screen fails the build, with the details in the job
summary; the smoke test draws the focus without its moving glow
(`PLAYGUARD_SIM_STILL_FOCUS`), so a screen nobody changed comes out the same.

A UI change that is meant:

```sh
make desktop
xvfb-run -s "-screen 0 1280x720x24" python3 tools/desktop_smoke.py smoke
python3 tools/visual_check.py smoke tests/visual/smoke --update
```

(`smoke-errors … errors`, `smoke-gate … gate`, `smoke-rescue … rescue`, `smoke-forged … forged` and `smoke-lock … lock` the same way), then look at
the new images before committing them. `--add NAME …` adds a screen to a set;
keep out screens with a toast, a held-key scroll, the app version or
SD-card content (backups) that piles up between local runs.
