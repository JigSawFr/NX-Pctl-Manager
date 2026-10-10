# playguard-rescue — boot sysmodule (see source/main.c).
# Produces out/playguard-rescue.nsp (exefs: main + main.npdm), which the
# release zip places at atmosphere/contents/4200000000505247/exefs.nsp and
# PlayGuard bundles in its romfs (Tools › Optional modules).

# TOPDIR: this folder, in both passes (the second runs from build/).
TOPDIR      ?= $(CURDIR)
TARGET      := playguard-rescue
# main.c lives here; rescue.c is the pure core shared with the app.
SOURCES     := source ../../source/core
INCLUDES    := ../../source/core
APP_JSON    := $(TOPDIR)/playguard-rescue.json

# Only these sources — ../../source/core also holds the app's libnx service
# layer, which this sysmodule must not pull in.
CFILES      := main.c rescue.c

include $(TOPDIR)/../common.mk
