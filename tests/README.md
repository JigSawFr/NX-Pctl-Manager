# Host tests

Plain-gcc unit tests of the C service layer and of the UI-free C++ helpers.
The C suites compile the real source against a small fake `<switch.h>`
(`-DNX_HOST_TEST`) that models service ownership, firmware version, command
payloads and buffer descriptors — not Horizon IPC encoding.

```sh
make test
```

| Suite | Covers |
|---|---|
| `pctl_session/` | `source/core/pctl_ops.c`: one session per operation and release on every error path; firmware gates (no play-timer IPC below 21.0.0, 1459 on 20.0.0+, 1460 on 23.0.0+); the write gate for all enabled / restricted / unlocked combinations and every play-timer write (the three limit entry points, the alarm switch, start and stop); the temporary-unlock verification, the PIN it hands to 1201 and the lock (1007) sent when 1006 cannot be read after it, and `NXM_RC_RELOCK_FAILED` when that lock fails too; the play-timer write starting from what 145601 returns (unknown fields kept, the same limits giving the same block byte for byte, the observed layout when the timer was off, or when the read fails while it is off — while it is active a failed read refuses the write, `NXM_RC_STATE_UNKNOWN`, unless no day keeps a limit); `pctl_get_pin` (4 to 8 digits only, output wiped on every error); restriction writes; the PIN never appearing in the dump; read-only mode refusing every mutation, and writes working again once it is off. |
| `time_ops/` | `source/core/time_ops.c`: handle ownership, each clock / flag / time-zone failure, the automatic-correction gate, read-back verification and its overflow bound, local-time formatting, read-only mode. |
| `sysinfo/` | `source/core/sysinfo.c`: Atmosphère version decoding, emuMMC and PRODINFO-blank detection (spl 65007 / 65005, with the blanked serial as fallback), the serial number, spl and set:sys session release, applet detection, the compatibility policy. |
| `patches/` | `source/util/patches.cpp`: sys-patch `log.ini` parsing, stale logs (other firmware or storage), sigpatch file detection, and the verdict (sys-patch, incomplete, files only, none) on fake SD card trees. |
| `backup/` | `source/util/backup.cpp`: the settings-backup JSON round trip, values that could not be read left out, every out-of-range or mistyped value refusing the whole file, file naming, newest-first listing, damaged files, and pruning that never deletes the backup just written (made with the clock in the past, it sorts oldest). Needs the borealis submodule (nlohmann/json). |
| `table_export/` | `source/util/table_export.cpp`: the Activity export in CSV (byte-order mark, quoting, formula-looking text kept as text, tab / carriage return included; non-integer numeric cells as text), JSON (numbers, nulls, column order), XLSX (each zip entry checked with an independent CRC-32, cells, escaping, invalid UTF-8 and characters XML refuses, sheet name cut on whole characters without leading or trailing apostrophes) and PDF (cross-reference offsets, stream lengths, WinAnsi text, page breaks), and file naming. Needs the borealis submodule (nlohmann/json). |
| `update/` | `source/util/update.cpp`: version comparison (numeric parts, `v` prefix, `-dev` builds before releases), firmware strings, `compat.json` parsing (missing, mistyped or malformed fields refuse the file) and the decision of the firmware screen (update supports the firmware, does not yet, already the latest). Needs the borealis submodule (nlohmann/json). |
| `config/` | `source/util/config.cpp` and the `.tmp` recovery of `paths.cpp`: each field read on its own, unknown or out-of-range values (64-bit ones included, never narrowed) back to their defaults, the firmware choice and the extra-time record dropped as a whole when one of their fields is wrong, the round trip, a save that stopped between its remove and its rename, and the next save promoting that `.tmp` before rewriting it (even when that save fails). Needs the borealis submodule (nlohmann/json). |
| `duration/` | `source/util/duration.cpp`: duration input (`90`, `1:30`, `2h`, `1h30m`), the 24 h bound and malformed entries (`:30`, `1:60`, `2:001`, `1.5`). |
| `playlog/` | `source/util/playlog.c`: play time per game from the play-event log: focus / out-of-focus pairs, sessions cut short (HOME menu after a crash, sleep, another game), repeated or stray events, today and 7-day windows across midnight and the week start, steady clock versus a changed user clock, sessions over 24 h dropped, a game still in focus counted until now, the output limit; one user account's time (the game has the focus and that account open; a game launch closes the previous one's accounts; local multiplayer). |
| `history/` | `source/util/history.cpp`: the change-history round trip, newest first, trimming to the newest 200, damaged files and entries skipped (64-bit values refused, not narrowed), which entries can be undone (each value within its kind's range, as a backup restore accepts). Needs the borealis submodule (nlohmann/json). |
| `log_upload/` | `source/util/log_upload.cpp`: the debug files and saved reports offered (only date-named reports, newest first), the bundle and its cut under the size limit on a whole UTF-8 character, dpaste.org answers (only an `https://dpaste.org/<letters and digits>` link is taken), the form sent, percent-encoding and the prefilled bug-report link. |
| `rescue/` | `source/core/rescue.c`: the request's mode (delete only when a line, trimmed of spaces and a leading byte-order mark, reads `delete` in any case; `delete everything`, `undelete` or an empty file unlock), the report's format and parsing (round trip, any line order, Windows line ends, the optional `request=` line, malformed values refused). |
| `ntp_packet/` | `source/util/ntp_packet.c`: every malformed / unsynchronised NTP reply is rejected; NTP era handling. |

The `pctl_session`, `time_ops` and `ntp_packet` suites started from anbingxi's
fork (`diag/fw22-5-readonly`). Passing them does not prove how a real console
behaves; `tools/desktop_smoke.py` exercises the UI, and hardware testing is
still required for firmware-specific behaviour. `visual/` holds the reference
screenshots CI compares the smoke test's screens with (`tools/visual_check.py`,
see `visual/README.md`).
