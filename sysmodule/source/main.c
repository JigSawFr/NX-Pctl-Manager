// playguard-rescue — a tiny boot sysmodule: the "last chance" for a parent who
// forgot the parental-control PIN while the play timer keeps every program
// (PlayGuard included) from starting.
//
// At boot it looks for sd:/switch/playguard/RESCUE (see core/rescue.h). If it
// is there it removes it (used once; renamed RESCUE.done should that fail),
// then, with the PIN the console stores:
//   - unlocks parental controls temporarily (1208 reads the PIN, 1201 unlocks)
//     so the HOME menu and PlayGuard can start;
//   - or, when a line of the file reads "delete", deletes every parental
//     control (1043) — only once the request is gone, so it never repeats.
// It leaves the outcome in rescue_report.txt for PlayGuard, then exits — it
// holds no service and no memory for the rest of the session.
//
// This needs no applet and no screen, which is why it can run while everything
// else is blocked. It only reads a PIN the console already stores; it never
// writes one anywhere. The pctl command IDs and the buffer details (HIPC
// pointer buffers, the NUL-terminated PIN) are the ones in source/core/
// pctl_ops.c, proven on hardware.
//
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <switch.h>

#include "rescue.h"

#define DATA_DIR    "sdmc:/switch/playguard"
#define PATH_REQ    DATA_DIR "/" RESCUE_REQUEST_NAME
#define PATH_REQTXT DATA_DIR "/" RESCUE_REQUEST_NAME_TXT
#define PATH_DONE   DATA_DIR "/" RESCUE_REQUEST_NAME_DONE
#define PATH_REPORT DATA_DIR "/" RESCUE_REPORT_NAME

// A sysmodule, not an application: no applet, one fs session, a small heap.
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

#define INNER_HEAP_SIZE 0x40000
char g_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;
    fake_heap_start = g_heap;
    fake_heap_end   = g_heap + sizeof(g_heap);
}

// Whether sdmc: is mounted. Not an abort when it is not: without the SD card
// there is no request to read and no report to leave, so main() does nothing.
static bool s_sd_mounted;

void __appInit(void)
{
    Result rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
    s_sd_mounted = R_SUCCEEDED(fsdevMountSdmc());
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    smExit();
}

// ---- the pctl commands this module needs (source/core/pctl_ops.c) ----

static Result pctl_cmd(u32 id)
{
    return serviceDispatch(pctlGetServiceSession_Service(), id);
}

static Result pctl_pin_length(u32* len)
{
    return serviceDispatchOut(pctlGetServiceSession_Service(), 1206, *len);
}

static Result pctl_is_unlocked(bool* out)
{
    u8 v = 0;
    Result rc = serviceDispatchOut(pctlGetServiceSession_Service(), 1006, v);
    if (R_SUCCEEDED(rc)) *out = v != 0;
    return rc;
}

// 1208 GetPinCode into `pin` (always NUL-terminated); 1201 then unlocks with
// it. The PIN stays in this one buffer, wiped before returning; it is never
// written to the SD card.
static Result pctl_unlock_with_stored_pin(void)
{
    char pin[32];
    memset(pin, 0, sizeof(pin));
    u32 len = 0;
    Result rc = serviceDispatchOut(pctlGetServiceSession_Service(), 1208, len,
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_Out },
        .buffers      = { { pin, sizeof(pin) } });
    pin[sizeof(pin) - 1] = '\0';
    if (R_SUCCEEDED(rc)) {
        size_t n = 0;
        while (n < sizeof(pin) && pin[n]) n++;
        if (len > 0 && len < n) n = len;
        rc = serviceDispatch(pctlGetServiceSession_Service(), 1201,
            .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_In },
            .buffers      = { { pin, n + 1 } });   // NUL-terminated, like the app
    }
    memset(pin, 0, sizeof(pin));
    return rc;
}

// ---- SD files ----

static bool read_small(const char* path, char* buf, size_t size, size_t* out_len)
{
    *out_len = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, size - 1, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = n;
    return true;
}

// The request, under either name. Returns true and fills *mode when one is
// there.
static bool take_request(RescueMode* mode)
{
    static char buf[512];
    size_t n = 0;
    if (!read_small(PATH_REQ, buf, sizeof(buf), &n) && !read_small(PATH_REQTXT, buf, sizeof(buf), &n))
        return false;
    *mode = rescue_request_mode(buf, n);
    return true;
}

static bool exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// Takes one request file away so it is acted on once: removed, or, should that
// fail, renamed RESCUE.done (replacing an earlier one). Kept when it is still
// there afterwards; Removed too when it was never there.
static RescueRequest consume(const char* path)
{
    if (remove(path) == 0 || !exists(path)) return RescueRequest_Removed;
    remove(PATH_DONE);
    if (rename(path, PATH_DONE) == 0 && !exists(path)) return RescueRequest_Renamed;
    return RescueRequest_Kept;
}

// Both names (either may be there, or both): the worst outcome of the two.
static RescueRequest consume_requests(void)
{
    RescueRequest a = consume(PATH_REQ);
    RescueRequest b = consume(PATH_REQTXT);
    return a > b ? a : b;
}

static void write_report(const RescueReport* r)
{
    char buf[128];
    size_t n = rescue_report_format(r, buf, sizeof(buf));
    if (n == 0) return;
    // Through a ".tmp" sibling, as the app's own files: a report cut short
    // (power off, SD card full) is never read as a whole one. PlayGuard reads
    // the ".tmp" when the report itself is missing (paths::read_file).
    // Owner-writable only (fopen would ask for 0666).
    const int fd = open(PATH_REPORT ".tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE* f = fd >= 0 ? fdopen(fd, "wb") : NULL;
    if (!f) {
        if (fd >= 0) close(fd);
        return;
    }
    const bool written = fwrite(buf, 1, n, f) == n;
    if (fclose(f) != 0 || !written) {
        remove(PATH_REPORT ".tmp");
        return;
    }
    remove(PATH_REPORT);
    rename(PATH_REPORT ".tmp", PATH_REPORT);
}

// pctl can take a moment to come up at boot; the SD card too. Try for a while.
static bool wait_for_sd(void)
{
    for (int i = 0; i < 40 && !s_sd_mounted; i++) {   // ~10 s
        svcSleepThread(250000000ULL);
        s_sd_mounted = R_SUCCEEDED(fsdevMountSdmc());
    }
    return s_sd_mounted;
}

static bool wait_for_pctl(void)
{
    for (int i = 0; i < 40; i++) {   // ~10 s
        if (R_SUCCEEDED(pctlInitialize())) return true;
        svcSleepThread(250000000ULL);
    }
    return false;
}

static void do_rescue(RescueMode mode, RescueReport* out)
{
    out->mode    = mode;
    out->result  = RescueResult_Failed;
    out->rc      = 0;
    out->unlocks = 0;

    if (mode == RescueMode_Delete) {
        Result rc = pctl_cmd(1043);   // DeleteSettings — irreversible
        out->rc     = rc;
        out->result = R_SUCCEEDED(rc) ? RescueResult_Ok : RescueResult_Failed;
        return;
    }

    u32 len = 0;
    Result rc = pctl_pin_length(&len);
    if (R_FAILED(rc)) { out->rc = rc; return; }
    if (len == 0) { out->result = RescueResult_NoPin; return; }

    rc = pctl_unlock_with_stored_pin();
    if (R_FAILED(rc)) { out->rc = rc; return; }
    out->unlocks = 1;

    // Confirm the unlock took; if the read fails, say so rather than claim it.
    bool unlocked = false;
    rc = pctl_is_unlocked(&unlocked);
    if (R_FAILED(rc)) { out->rc = rc; return; }
    out->result = unlocked ? RescueResult_Ok : RescueResult_Failed;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RescueMode mode;
    if (wait_for_sd() && take_request(&mode)) {
        // Take the request away first: it is acted on once, present or not next
        // boot. A delete still there would run again at every boot, so it is
        // not done; an unlock is (the parent needs PlayGuard to open), and the
        // report says the file is still there.
        const RescueRequest request = consume_requests();

        RescueReport report;
        if (mode == RescueMode_Delete && request == RescueRequest_Kept) {
            report.mode = mode;
            report.result = RescueResult_Refused;
            report.rc = 0;
            report.unlocks = 0;
        } else if (wait_for_pctl()) {
            do_rescue(mode, &report);
            pctlExit();
        } else {
            report.mode = mode;
            report.result = RescueResult_Failed;
            report.rc = MAKERESULT(Module_Libnx, LibnxError_InitFail_SM);
            report.unlocks = 0;
        }
        report.request = request;
        write_report(&report);
    }

    return 0;   // boot2 reclaims the process; nothing stays resident
}
