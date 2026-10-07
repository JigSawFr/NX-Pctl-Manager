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
| `pctl_session/` | `source/core/pctl_ops.c`: one session per operation and release on every error path; firmware gates (no play-timer IPC below 21.0.0, 1459 on 20.0.0+, 1460 on 23.0.0+); the write gate for all enabled / restricted / unlocked combinations and all three write entry points; the temporary-unlock verification and the PIN it hands to 1201; `pctl_get_pin` (4 to 8 digits only, output wiped on every error); restriction writes; the PIN never appearing in the dump; read-only mode refusing every mutation, and writes working again once it is off. |
| `time_ops/` | `source/core/time_ops.c`: handle ownership, each clock / flag / time-zone failure, the automatic-correction gate, read-back verification and its overflow bound, local-time formatting, read-only mode. |
| `sysinfo/` | `source/core/sysinfo.c`: Atmosphère version decoding, emuMMC and PRODINFO-blank detection (spl 65007 / 65005, with the blanked serial as fallback), the serial number, spl and set:sys session release, applet detection, the compatibility policy. |
| `patches/` | `source/util/patches.cpp`: sys-patch `log.ini` parsing, stale logs (other firmware or storage), sigpatch file detection, and the verdict (sys-patch, incomplete, files only, none) on fake SD card trees. |
| `backup/` | `source/util/backup.cpp`: the settings-backup JSON round trip, values that could not be read left out, every out-of-range or mistyped value refusing the whole file, file naming, newest-first listing and damaged files. Needs the borealis submodule (nlohmann/json). |
| `table_export/` | `source/util/table_export.cpp`: the Activity export in CSV (byte-order mark, quoting, formula-looking text kept as text), JSON (numbers, nulls, column order), XLSX (each zip entry checked with an independent CRC-32, cells, escaping, sheet name) and PDF (cross-reference offsets, stream lengths, WinAnsi text, page breaks), and file naming. Needs the borealis submodule (nlohmann/json). |
| `update/` | `source/util/update.cpp`: version comparison (numeric parts, `v` prefix, `-dev` builds before releases), firmware strings, `compat.json` parsing (missing, mistyped or malformed fields refuse the file) and the decision of the firmware screen (update supports the firmware, does not yet, already the latest). Needs the borealis submodule (nlohmann/json). |
| `duration/` | `source/util/duration.cpp`: duration input (`90`, `1:30`, `2h`, `1h30m`), the 24 h bound and malformed entries (`:30`, `1:60`, `2:001`, `1.5`). |
| `playlog/` | `source/util/playlog.c`: play time per game from the play-event log: focus / out-of-focus pairs, sessions cut short (HOME menu after a crash, sleep, another game), repeated or stray events, today and 7-day windows across midnight and the week start, steady clock versus a changed user clock, sessions over 24 h dropped, a game still in focus counted until now, the output limit. |
| `ntp_packet/` | `source/util/ntp_packet.c`: every malformed / unsynchronised NTP reply is rejected; NTP era handling. |

The `pctl_session`, `time_ops` and `ntp_packet` suites started from anbingxi's
fork (`diag/fw22-5-readonly`). Passing them does not prove how a real console
behaves; `tools/desktop_smoke.py` exercises the UI, and hardware testing is
still required for firmware-specific behaviour.
