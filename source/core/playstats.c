// PlayGuard — play data service layer (see playstats.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "playstats.h"

#include <stdlib.h>
#include <string.h>

#include "../util/playlog.h"
#include "calendar.h"
#include "time_ops.h"

#define RECORD_CHUNK 64
#define EVENT_CHUNK  256
#define DAY_S        86400u

// Names read earlier in this run of the app: reading a game's control data is
// the slow part, and names do not change while the app is open.
static struct { u64 id; char name[PLAYSTATS_NAME_LEN]; } s_names[PLAYSTATS_MAX];
static u32 s_name_count;

static GameStat *find_or_add(PlayStats *out, u64 id)
{
    for (u32 i = 0; i < out->count; i++)
        if (out->games[i].app_id == id) return &out->games[i];
    if (out->count == PLAYSTATS_MAX) return NULL;
    GameStat *g = &out->games[out->count++];
    memset(g, 0, sizeof(*g));
    g->app_id = id;
    return g;
}

// One raw log entry as a playlog event; false for entries that do not matter.
static bool convert(const PdmPlayEvent *p, PlayLogEvent *e)
{
    e->app_id    = 0;
    e->ts_user   = p->timestamp_user;
    e->ts_steady = p->timestamp_steady;

    if (p->play_event_type == PdmPlayEventType_PowerStateChange) {   // sleep, wake, shutdown
        e->kind = PlayLogEv_Away;
        return true;
    }
    if (p->play_event_type != PdmPlayEventType_Applet) return false;

    const u8 type = p->event_data.applet.event_type;
    if (p->event_data.applet.applet_id != AppletId_application) {
        // The HOME menu taking the focus: no game has it any more (this is
        // what ends a session whose game crashed without "out of focus").
        if (p->event_data.applet.applet_id == AppletId_SystemAppletMenu && type == PdmAppletEventType_InFocus) {
            e->kind = PlayLogEv_Away;
            return true;
        }
        return false;
    }
    // Same filter as the system's own play statistics.
    if (p->event_data.applet.log_policy != PdmPlayLogPolicy_All) return false;
    // The two halves of the ProgramId are stored swapped.
    e->app_id = ((u64)p->event_data.applet.program_id[0] << 32) | p->event_data.applet.program_id[1];
    switch (type) {
        case PdmAppletEventType_InFocus:
            e->kind = PlayLogEv_Focus;
            return true;
        case PdmAppletEventType_OutOfFocus:
        case PdmAppletEventType_OutOfFocus4:
        case PdmAppletEventType_Exit:
        case PdmAppletEventType_Exit5:
        case PdmAppletEventType_Exit6:
            e->kind = PlayLogEv_Unfocus;
            return true;
        case PdmAppletEventType_Launch:   // a new start: whatever was in focus is over
            e->app_id = 0;
            e->kind   = PlayLogEv_Away;
            return true;
        default:
            return false;
    }
}

// The log entries since `since` (user clock), oldest first. *events is
// malloc'ed (NULL when empty); the caller frees it.
static Result read_events(u64 since, PlayLogEvent **events, size_t *count)
{
    *events = NULL;
    *count  = 0;
    s32 total = 0, start = 0, end = 0;
    Result rc = pdmqryGetAvailablePlayEventRange(&total, &start, &end);
    if (R_FAILED(rc)) return rc;

    PdmPlayEvent *chunk = (PdmPlayEvent *)malloc(sizeof(PdmPlayEvent) * EVENT_CHUNK);
    if (!chunk) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    size_t cap = 0;
    s32 index = start, remaining = total;
    while (remaining > 0 && R_SUCCEEDED(rc)) {
        s32 got = 0;
        rc = pdmqryQueryPlayEvent(index, chunk, remaining < EVENT_CHUNK ? remaining : EVENT_CHUNK, &got);
        if (R_FAILED(rc) || got <= 0) break;
        index += got;
        remaining -= got;
        for (s32 i = 0; i < got; i++) {
            PlayLogEvent e;
            if (!convert(&chunk[i], &e) || e.ts_user < since) continue;
            if (*count == cap) {
                size_t grown = cap ? cap * 2 : 1024;
                PlayLogEvent *bigger = (PlayLogEvent *)realloc(*events, grown * sizeof(PlayLogEvent));
                if (!bigger) {
                    rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
                    break;
                }
                *events = bigger;
                cap = grown;
            }
            (*events)[(*count)++] = e;
        }
    }
    free(chunk);
    return rc;
}

// Copies at most n-1 bytes of a UTF-8 string without cutting a character.
static void copy_utf8(char *dst, size_t n, const char *src)
{
    size_t len = strnlen(src, n - 1);
    if (len == n - 1)
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80) len--;   // inside a character
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void read_name(GameStat *g, NsApplicationControlData *cd)
{
    for (u32 i = 0; i < s_name_count; i++) {
        if (s_names[i].id == g->app_id) {
            memcpy(g->name, s_names[i].name, sizeof(g->name));
            return;
        }
    }
    u64 size = 0;
    NacpLanguageEntry *lang = NULL;
    if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, g->app_id, cd, sizeof(*cd), &size)) ||
        size < sizeof(cd->nacp) || R_FAILED(nacpGetLanguageEntry(&cd->nacp, &lang)) || !lang)
        return;   // not installed any more: the UI shows the title ID
    copy_utf8(g->name, sizeof(g->name), lang->name);
    if (s_name_count < PLAYSTATS_MAX) {
        s_names[s_name_count].id = g->app_id;
        memcpy(s_names[s_name_count].name, g->name, sizeof(g->name));
        s_name_count++;
    }
}

void playstats_fetch(PlayStats *out)
{
    memset(out, 0, sizeof(*out));
    time_local_now(&out->now, NULL);   // live, unlike time() (calendar.h)

    out->rc = nsInitialize();
    if (R_FAILED(out->rc)) return;

    // Installed games.
    u64 *ids = NULL;
    size_t id_count = 0, id_cap = 0;
    NsApplicationRecord records[RECORD_CHUNK];
    for (s32 offset = 0;;) {
        s32 got = 0;
        Result rc = nsListApplicationRecord(records, RECORD_CHUNK, offset, &got);
        if (R_FAILED(rc)) {
            if (offset == 0) out->rc = rc;
            break;
        }
        if (got <= 0) break;
        for (s32 i = 0; i < got; i++) {
            if (id_count == id_cap) {
                size_t grown = id_cap ? id_cap * 2 : 128;
                u64 *bigger = (u64 *)realloc(ids, grown * sizeof(u64));
                if (!bigger) break;
                ids = bigger;
                id_cap = grown;
            }
            ids[id_count++] = records[i].application_id;
        }
        offset += got;
        if (got < RECORD_CHUNK) break;
    }
    if (R_FAILED(out->rc)) {
        free(ids);
        nsExit();
        return;
    }

    out->stats_rc  = pdmqryInitialize();
    out->events_rc = out->stats_rc;
    if (R_SUCCEEDED(out->stats_rc)) {
        // All-time totals of every installed game that was ever played.
        for (size_t i = 0; i < id_count; i++) {
            PdmPlayStatistics st;
            memset(&st, 0, sizeof(st));
            if (R_FAILED(pdmqryQueryPlayStatisticsByApplicationId(ids[i], false, &st))) continue;
            if (st.playtime == 0 && st.total_launches == 0) continue;
            GameStat *g = find_or_add(out, ids[i]);
            if (!g) break;
            g->totals_ok    = true;
            g->total_s      = st.playtime / 1000000000ULL;
            g->launches     = st.total_launches;
            g->first_played = st.first_timestamp_user;
            g->last_played  = st.last_timestamp_user;
        }

        // Today and the last 7 days. A session can start up to a day before
        // the window and still end inside it.
        // Midnights through the console's rule: a day with a daylight-saving
        // change is 23 or 25 h long, and the week counts it as one day.
        const u64 day_start  = local_midnight(time_console_rule(), out->now, 0);
        const u64 week_start = local_midnight(time_console_rule(), out->now, 6);
        PlayLogEvent *events = NULL;
        size_t event_count = 0;
        out->events_rc = read_events(week_start >= DAY_S ? week_start - DAY_S : 0, &events, &event_count);
        if (R_SUCCEEDED(out->events_rc)) {
            static PlayLogTotal totals[PLAYSTATS_MAX];
            const size_t n = playlog_fold(events, event_count, out->now, day_start, week_start,
                                          totals, PLAYSTATS_MAX);
            for (size_t i = 0; i < n; i++) {
                GameStat *g = find_or_add(out, totals[i].app_id);   // also games deleted since
                if (!g) break;
                g->today_s = totals[i].today_s;
                g->week_s  = totals[i].week_s;
            }
            out->windows_ok = true;
        }
        free(events);
        pdmqryExit();
    }
    free(ids);

    NsApplicationControlData *cd = (NsApplicationControlData *)malloc(sizeof(NsApplicationControlData));
    if (cd) {
        for (u32 i = 0; i < out->count; i++) read_name(&out->games[i], cd);
        free(cd);
    }
    nsExit();
}
