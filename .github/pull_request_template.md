<!--
The title must be a conventional commit (feat: …, fix: …, docs: …): CI checks it, and the
squash merge makes it the commit subject that release-please writes the changelog from.
-->

## What this changes, and why

<!-- The diff says what; this says what it is for. Link the issue it closes, if any. -->

## How it was verified

<!-- Beyond CI. Say whether it ran on a console (firmware, Atmosphère, launcher) or only in `make desktop`. -->

## Checklist

- [ ] `make test` and `python3 tools/check_resources.py .` pass
- [ ] New strings are in every `resources/i18n/*/playguard.json` (English is fine as a placeholder)
- [ ] A screen that changes on purpose has its `tests/visual` reference updated
- [ ] A change users can see is in both README.md and README.fr.md
- [ ] Nothing writes the play timer while it counts down
