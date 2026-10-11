# Thin shim over CMake — the real build is in CMakeLists.txt.
#
#   make              -> ./playguard.nro            (needs devkitPro, DEVKITPRO set)
#   make dist         -> ./playguard.zip            (unzip onto the SD card root)
#   make desktop      -> ./build-desktop/playguard  (UI with a simulated backend; needs GLFW)
#   make test         -> host unit tests of the C service layer (plain gcc, ASan + UBSan)
#   make nxlink       -> push to a Switch running hbmenu (press Y there first)
#
# GL=1 builds the Switch .nro with OpenGL (mesa) instead of deko3d.
# JOBS=n sets the parallel build jobs (default: every core). SAN= turns the
# sanitizers of the host tests off (e.g. a compiler without them). COV=--coverage
# builds them for gcov (CI: with gcc and SAN=, summarised by gcovr).
# SOURCE_DATE_EPOCH (default: the last commit's time) dates every file of the
# release zips, so the same commit gives the same zip.
# CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER=ccache in the
# environment are picked up by CMake (CI uses them). CMAKE_ARGS=... is passed
# to the configure step of `make` (so `make dist`) and `make desktop` (CI
# builds both with CMAKE_ARGS=-DPLAYGUARD_WERROR=ON, warnings as errors).

TARGET  := playguard
BUILD   := build
DESKTOP := build-desktop
CC      ?= gcc
CXX     ?= g++
TESTOUT := $(BUILD)/host-tests
JOBS    ?= $(shell nproc 2>/dev/null || echo 4)
SAN     ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined
COV     ?=
# -Itests: tests/check.h, the CHECK() every suite uses.
CWARN   := -Wall -Wextra -Werror $(SAN) $(COV) -Itests
SOURCE_DATE_EPOCH ?= $(shell git log -1 --format=%ct 2>/dev/null || echo 0)
# A zip that depends only on the files: one date for all, entries in a fixed
# order, no extra file attributes (-X), and TZ=UTC as a zip keeps local times.
# Run inside the tree, the zip file last.
ZIP_TREE = find . -exec touch -h -d @$(SOURCE_DATE_EPOCH) {} + && find . -mindepth 1 | LC_ALL=C sort | TZ=UTC zip -X -@
CMAKE_ARGS ?=
JSON_HPP := extern/borealis/library/include/borealis/extern/nlohmann/json.hpp

.PHONY: all clean dist nxlink desktop test check rescue dist-rescue

RENDERER := $(if $(GL),-DUSE_DEKO3D=OFF,-DUSE_DEKO3D=ON)

all:
	@cmake -B $(BUILD) -S . -DPLATFORM_SWITCH=ON $(RENDERER) $(CMAKE_ARGS)
	@cmake --build $(BUILD) --target $(TARGET).nro -j $(JOBS)
	@cp $(BUILD)/$(TARGET).nro  $(TARGET).nro
	@cp $(BUILD)/$(TARGET).nacp $(TARGET).nacp

# Layout on the SD card: one folder per app (hbmenu / hb-appstore / sphaira
# convention) + the sphaira GitHub-updater entry. The GPL text travels with
# the binary.
dist: all
	@echo making dist ...
	@rm -rf out/ $(TARGET).zip
	@mkdir -p out/switch/$(TARGET) out/config/sphaira/github
	@cp $(BUILD)/$(TARGET).nro out/switch/$(TARGET)/
	@cp LICENSE out/switch/$(TARGET)/LICENSE.txt
	@cp packaging/sphaira/$(TARGET).json out/config/sphaira/github/
	@cd out && $(ZIP_TREE) ../$(TARGET).zip

# The optional recovery sysmodule (sysmodule/), as its own asset so it is a
# deliberate install, never part of the default one. The zip drops into the
# SD-card root: Atmosphère runs 4200000000505247 at boot (the boot2.flag).
RESCUE_TID := 4200000000505247
rescue:
	@$(MAKE) --no-print-directory -C sysmodule

dist-rescue: rescue
	@echo making rescue dist ...
	@rm -rf out-rescue/ playguard-rescue.zip
	@mkdir -p out-rescue/atmosphere/contents/$(RESCUE_TID)/flags
	@cp sysmodule/out/playguard-rescue.nsp out-rescue/atmosphere/contents/$(RESCUE_TID)/exefs.nsp
	@touch out-rescue/atmosphere/contents/$(RESCUE_TID)/flags/boot2.flag
	@cp LICENSE out-rescue/atmosphere/contents/$(RESCUE_TID)/LICENSE.txt
	@cd out-rescue && $(ZIP_TREE) ../playguard-rescue.zip

desktop:
	@cmake -B $(DESKTOP) -S . -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)
	@cmake --build $(DESKTOP) -j $(JOBS)

test:
	@test -f $(JSON_HPP) || { echo "borealis is missing (nlohmann/json for the tests): run git submodule update --init"; exit 1; }
	@mkdir -p $(TESTOUT)
	$(CC) -std=gnu11 $(CWARN) -DNX_HOST_TEST -Itests/pctl_session -Isource/core source/core/pctl_ops.c source/core/pure.c source/core/write_guard.c tests/pctl_session/test.c -o $(TESTOUT)/pctl && $(TESTOUT)/pctl
	$(CC) -std=gnu11 $(CWARN) -DNX_HOST_TEST -Itests/time_ops -Isource/core source/core/time_ops.c source/core/calendar.c source/core/write_guard.c tests/time_ops/test.c -o $(TESTOUT)/time && $(TESTOUT)/time
	$(CC) -std=gnu11 $(CWARN) -DNX_HOST_TEST -Itests/sysinfo -Isource/core source/core/sysinfo.c source/core/pure.c tests/sysinfo/test.c -o $(TESTOUT)/sysinfo && $(TESTOUT)/sysinfo
	$(CC) -std=gnu11 $(CWARN) -Isource/core source/core/calendar.c tests/calendar/test.c -o $(TESTOUT)/calendar && $(TESTOUT)/calendar
	$(CC) -std=c11 $(CWARN) -Isource/core source/core/rescue.c tests/rescue/test.c -o $(TESTOUT)/rescue && $(TESTOUT)/rescue
	$(CC) -std=c11 $(CWARN) -Isource/util source/util/ntp_packet.c tests/ntp_packet/test.c -o $(TESTOUT)/ntp && $(TESTOUT)/ntp
	$(CC) -std=c11 $(CWARN) -Isource/util source/util/playlog.c tests/playlog/test.c -o $(TESTOUT)/playlog && $(TESTOUT)/playlog
	$(CC) -std=gnu11 $(CWARN) -DNX_HOST_TEST -Itests/playstats -Isource/core source/core/playstats_logic.c tests/playstats/test.c -lz -o $(TESTOUT)/playstats && $(TESTOUT)/playstats
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/paths.cpp source/util/patches.cpp tests/patches/test.cpp -o $(TESTOUT)/patches && $(TESTOUT)/patches
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/duration.cpp tests/duration/test.cpp -o $(TESTOUT)/duration && $(TESTOUT)/duration
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/changelog.cpp tests/changelog/test.cpp -o $(TESTOUT)/changelog && $(TESTOUT)/changelog
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/support.cpp tests/support/test.cpp -o $(TESTOUT)/support && $(TESTOUT)/support
	$(CXX) -std=c++17 $(CWARN) -Isource source/action/pt_logic.cpp tests/pt_logic/test.cpp -o $(TESTOUT)/pt_logic && $(TESTOUT)/pt_logic
	$(CXX) -std=c++17 $(CWARN) -Isource source/action/pin_lock_logic.cpp tests/pin_lock_logic/test.cpp -o $(TESTOUT)/pin_lock_logic && $(TESTOUT)/pin_lock_logic
	$(CXX) -std=c++17 $(CWARN) -Isource source/action/console_lock_logic.cpp tests/console_lock_logic/test.cpp -o $(TESTOUT)/console_lock_logic && $(TESTOUT)/console_lock_logic
	$(CXX) -std=c++17 $(CWARN) -Isource source/action/history_logic.cpp tests/history_logic/test.cpp -o $(TESTOUT)/history_logic && $(TESTOUT)/history_logic
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/backup.cpp tests/backup/test.cpp -o $(TESTOUT)/backup && $(TESTOUT)/backup
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/table_export.cpp tests/table_export/test.cpp -o $(TESTOUT)/table_export && $(TESTOUT)/table_export
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/update.cpp tests/update/test.cpp -o $(TESTOUT)/update && $(TESTOUT)/update
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/config.cpp tests/config/test.cpp -o $(TESTOUT)/config && $(TESTOUT)/config
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/profiles.cpp tests/profiles/test.cpp -o $(TESTOUT)/profiles && $(TESTOUT)/profiles
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/pt_block.cpp tests/pt_block/test.cpp -o $(TESTOUT)/pt_block && $(TESTOUT)/pt_block
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/paths.cpp source/util/launcher.cpp tests/launcher/test.cpp -o $(TESTOUT)/launcher && $(TESTOUT)/launcher
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/history.cpp tests/history/test.cpp -o $(TESTOUT)/history && $(TESTOUT)/history
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/pt_log.cpp source/util/log_upload.cpp tests/log_upload/test.cpp -o $(TESTOUT)/log_upload && $(TESTOUT)/log_upload
	$(CXX) -std=c++17 $(CWARN) -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/sha256.cpp source/util/dev_builds.cpp source/util/zip_read.cpp source/util/github_auth.cpp tests/dev_builds/test.cpp -lz -o $(TESTOUT)/dev_builds && $(TESTOUT)/dev_builds
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/play_cache.cpp tests/play_cache/test.cpp -o $(TESTOUT)/play_cache && $(TESTOUT)/play_cache
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/activity_summary.cpp tests/activity_summary/test.cpp -o $(TESTOUT)/activity_summary && $(TESTOUT)/activity_summary
	$(CXX) -std=c++17 $(CWARN) -Isource source/util/paths.cpp source/util/pt_log.cpp tests/pt_log/test.cpp -o $(TESTOUT)/pt_log && $(TESTOUT)/pt_log

check: test
	python3 tools/check_resources.py .
	python3 tools/gen_compat.py . $(BUILD)/compat.json

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(DESKTOP) out out-rescue $(TARGET).zip playguard-rescue.zip $(TARGET).nro $(TARGET).nacp $(TARGET).elf
	@$(MAKE) --no-print-directory -C sysmodule clean 2>/dev/null || true

nxlink: all
	nxlink $(BUILD)/$(TARGET).nro
