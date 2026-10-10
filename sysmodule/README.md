# PlayGuard's optional sysmodules

Two small Atmosphère sysmodules, both **optional**: PlayGuard works fully
without them. PlayGuard carries a copy of each in its romfs and installs,
updates, starts at boot or removes them from *Tools › Optional modules*
(`source/util/modules.hpp`, `source/activity/modules_activity.cpp`); each is
also a download of its own on the releases page.

| Folder | Program id | What it does |
|---|---|---|
| [`rescue/`](rescue/README.md) | `4200000000505247` | At boot, only when a `RESCUE` file asks it to: unlocks parental controls with the stored PIN (or deletes them), for a forgotten PIN with a 0-minute limit. Then exits. |
| [`agent/`](agent/README.md) | `4200000000504741` | Keeps the remote link (MQTT, Home Assistant) up while PlayGuard is closed; PlayGuard talks to it over `pg:agent` while it runs. |

Both are built with [`common.mk`](common.mk) (devkitPro's libnx rules, the ARM
flags, `out/<name>.nsp`):

```sh
make rescue          # sysmodule/rescue/out/playguard-rescue.nsp
make dist-rescue     # playguard-rescue.zip
make agent           # sysmodule/agent/out/playguard-agent.nsp
make dist-agent      # playguard-agent.zip
make                 # the .nro, with the modules built first and bundled (cmake/bundle_sysmodules.cmake)
```
