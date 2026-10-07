# Host tests

Plain-gcc unit tests of the C service layer. Each one compiles the real source
against a small fake `<switch.h>` (`-DNX_HOST_TEST`) that models service
ownership, firmware version and command payloads — not Horizon IPC encoding.

```sh
make test
```

| Suite | Covers |
|---|---|
| `pctl_session/` | `source/core/pctl_ops.c`: one session per operation and release on every error path; firmware gates (no play-timer IPC below 21.0.0, 1459 on 20.0.0+, 1460 on 23.0.0+); the write gate for all enabled / restricted / unlocked combinations and all three write entry points; the temporary-unlock verification; restriction writes; the PIN never appearing in the dump; READ_ONLY builds refusing every mutation. |
| `time_ops/` | `source/core/time_ops.c`: handle ownership, each clock / flag / time-zone failure, the automatic-correction gate, read-back verification and its overflow bound, local-time formatting, READ_ONLY. |
| `sysinfo/` | `source/core/sysinfo.c`: Atmosphère version decoding, spl session release, applet detection, the compatibility policy. |
| `ntp_packet/` | `source/util/ntp_packet.c`: every malformed / unsynchronised NTP reply is rejected; NTP era handling. |

The `pctl_session`, `time_ops` and `ntp_packet` suites started from anbingxi's
fork (`diag/fw22-5-readonly`). Passing them does not prove how a real console
behaves; `tools/desktop_smoke.py` exercises the UI, and hardware testing is
still required for firmware-specific behaviour.
