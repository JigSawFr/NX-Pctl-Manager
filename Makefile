# Thin shim over CMake — the real build is in CMakeLists.txt.
#
#   make              -> ./playguard.nro            (needs devkitPro, DEVKITPRO set)
#   make dist         -> ./playguard.zip            (unzip onto the SD card root)
#   make desktop      -> ./build-desktop/playguard  (UI with a simulated backend; needs GLFW)
#   make test         -> host unit tests of the C service layer (plain gcc)
#   make nxlink       -> push to a Switch running hbmenu (press Y there first)

TARGET  := playguard
BUILD   := build
DESKTOP := build-desktop
CC      ?= gcc
CXX     ?= g++
TESTOUT := $(BUILD)/host-tests

.PHONY: all clean dist nxlink desktop test check

all:
	@cmake -B $(BUILD) -S . -DPLATFORM_SWITCH=ON
	@cmake --build $(BUILD) --target $(TARGET).nro
	@cp $(BUILD)/$(TARGET).nro  $(TARGET).nro
	@cp $(BUILD)/$(TARGET).nacp $(TARGET).nacp

# Layout on the SD card: one folder per app (hbmenu / hb-appstore / sphaira
# convention) + the sphaira GitHub-updater entry.
dist: all
	@echo making dist ...
	@rm -rf out/ $(TARGET).zip
	@mkdir -p out/switch/$(TARGET) out/config/sphaira/github
	@cp $(BUILD)/$(TARGET).nro out/switch/$(TARGET)/
	@cp packaging/sphaira/$(TARGET).json out/config/sphaira/github/
	@cd out && zip -r ../$(TARGET).zip ./*

desktop:
	@cmake -B $(DESKTOP) -S . -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release
	@cmake --build $(DESKTOP) -j

test:
	@mkdir -p $(TESTOUT)
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/pctl_session -Isource/core source/core/pctl_ops.c source/core/write_guard.c tests/pctl_session/test.c -o $(TESTOUT)/pctl && $(TESTOUT)/pctl
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/time_ops -Isource/core source/core/time_ops.c source/core/write_guard.c tests/time_ops/test.c -o $(TESTOUT)/time && $(TESTOUT)/time
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/sysinfo -Isource/core source/core/sysinfo.c tests/sysinfo/test.c -o $(TESTOUT)/sysinfo && $(TESTOUT)/sysinfo
	$(CC) -std=c11 -Wall -Wextra -Werror -Isource/util source/util/ntp_packet.c tests/ntp_packet/test.c -o $(TESTOUT)/ntp && $(TESTOUT)/ntp
	$(CC) -std=c11 -Wall -Wextra -Werror -Isource/util source/util/playlog.c tests/playlog/test.c -o $(TESTOUT)/playlog && $(TESTOUT)/playlog
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource source/util/paths.cpp source/util/patches.cpp tests/patches/test.cpp -o $(TESTOUT)/patches && $(TESTOUT)/patches
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource source/util/duration.cpp tests/duration/test.cpp -o $(TESTOUT)/duration && $(TESTOUT)/duration
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/backup.cpp tests/backup/test.cpp -o $(TESTOUT)/backup && $(TESTOUT)/backup
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/table_export.cpp tests/table_export/test.cpp -o $(TESTOUT)/table_export && $(TESTOUT)/table_export
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource -Iextern/borealis/library/include source/util/update.cpp tests/update/test.cpp -o $(TESTOUT)/update && $(TESTOUT)/update

check: test
	python3 tools/check_resources.py .
	python3 tools/gen_compat.py . $(BUILD)/compat.json

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(DESKTOP) out $(TARGET).zip $(TARGET).nro $(TARGET).nacp $(TARGET).elf

nxlink: all
	nxlink $(BUILD)/$(TARGET).nro
