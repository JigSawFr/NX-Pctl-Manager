# Thin shim over CMake — the real build is in CMakeLists.txt.
#
#   make              -> ./playguard.nro            (needs devkitPro, DEVKITPRO set)
#   make dist         -> ./playguard.zip            (unzip onto the SD card root)
#   make PROBE=1      -> extra diagnostic shortcuts
#   make READ_ONLY=1  -> "PlayGuard Diagnostics" build that cannot change anything
#   make desktop      -> ./build-desktop/playguard  (UI with a simulated backend; needs GLFW)
#   make test         -> host unit tests of the C service layer (plain gcc)
#   make nxlink       -> push to a Switch running hbmenu (press Y there first)

TARGET  := playguard
BUILD   := build
DESKTOP := build-desktop
CC      ?= gcc
CXX     ?= g++
TESTOUT := $(BUILD)/host-tests

# Always pass both flags explicitly so toggling them updates the CMake cache.
CMAKE_FLAGS :=
ifeq ($(strip $(PROBE)),1)
	CMAKE_FLAGS += -DPCTL_PROBE=ON
else
	CMAKE_FLAGS += -DPCTL_PROBE=OFF
endif
ifeq ($(strip $(READ_ONLY)),1)
	CMAKE_FLAGS += -DPCTL_READ_ONLY=ON
else
	CMAKE_FLAGS += -DPCTL_READ_ONLY=OFF
endif

.PHONY: all clean dist nxlink desktop test check

all:
	@cmake -B $(BUILD) -S . -DPLATFORM_SWITCH=ON $(CMAKE_FLAGS)
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
	@cmake -B $(DESKTOP) -S . -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS)
	@cmake --build $(DESKTOP) -j

test:
	@mkdir -p $(TESTOUT)
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/pctl_session -Isource/core source/core/pctl_ops.c tests/pctl_session/test.c -o $(TESTOUT)/pctl && $(TESTOUT)/pctl
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -DPCTL_READ_ONLY=1 -Itests/pctl_session -Isource/core source/core/pctl_ops.c tests/pctl_session/test.c -o $(TESTOUT)/pctl_ro && $(TESTOUT)/pctl_ro
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/time_ops -Isource/core source/core/time_ops.c tests/time_ops/test.c -o $(TESTOUT)/time && $(TESTOUT)/time
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -DPCTL_READ_ONLY=1 -Itests/time_ops -Isource/core source/core/time_ops.c tests/time_ops/test.c -o $(TESTOUT)/time_ro && $(TESTOUT)/time_ro
	$(CC) -std=gnu11 -Wall -Wextra -Werror -DNX_HOST_TEST -Itests/sysinfo -Isource/core source/core/sysinfo.c tests/sysinfo/test.c -o $(TESTOUT)/sysinfo && $(TESTOUT)/sysinfo
	$(CC) -std=c11 -Wall -Wextra -Werror -Isource/util source/util/ntp_packet.c tests/ntp_packet/test.c -o $(TESTOUT)/ntp && $(TESTOUT)/ntp
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource source/util/paths.cpp source/util/patches.cpp tests/patches/test.cpp -o $(TESTOUT)/patches && $(TESTOUT)/patches
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource source/util/duration.cpp tests/duration/test.cpp -o $(TESTOUT)/duration && $(TESTOUT)/duration
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Isource -Iextern/borealis/library/include source/util/paths.cpp source/util/backup.cpp tests/backup/test.cpp -o $(TESTOUT)/backup && $(TESTOUT)/backup

check: test
	python3 tools/check_resources.py .

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(DESKTOP) out $(TARGET).zip $(TARGET).nro $(TARGET).nacp $(TARGET).elf

nxlink: all
	nxlink $(BUILD)/$(TARGET).nro
