// PlayGuard — installed games check, console side (see gamecheck.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "gamecheck.h"

#include <stdlib.h>
#include <string.h>

#include "write_guard.h"

#define RECORD_CHUNK  64
#define META_CHUNK    16
#define CONTENT_CHUNK 16

// The storages a game can be installed on, each opened once per scan.
typedef struct {
    NcmStorageId id;
    bool tried, ok;
    NcmContentMetaDatabase db;
    NcmContentStorage cs;
} Storage;

static Storage *storage_for(Storage *list, size_t n, NcmStorageId id)
{
    for (size_t i = 0; i < n; i++) {
        Storage *s = &list[i];
        if (s->id != id) continue;
        if (!s->tried) {
            s->tried = true;
            if (R_SUCCEEDED(ncmOpenContentMetaDatabase(&s->db, id))) {
                if (R_SUCCEEDED(ncmOpenContentStorage(&s->cs, id))) s->ok = true;
                else ncmContentMetaDatabaseClose(&s->db);
            }
        }
        return s->ok ? s : NULL;
    }
    return NULL;
}

static void storages_close(Storage *list, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (!list[i].ok) continue;
        ncmContentStorageClose(&list[i].cs);
        ncmContentMetaDatabaseClose(&list[i].db);
    }
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

// One content (the game or its update) on its storage: known there, its
// files present, the system version it asks for.
static void check_meta(GameFacts *g, const NsApplicationContentMetaStatus *m, Storage *storages, size_t n)
{
    Storage *s = storage_for(storages, n, (NcmStorageId)m->storageID);
    if (!s) {
        g->storage_unavailable = true;
        return;
    }
    NcmContentMetaKey key;
    memset(&key, 0, sizeof(key));
    key.id           = m->application_id;   // the content's own ID (an update's ends in 800)
    key.version      = m->version;
    key.type         = m->meta_type;
    key.install_type = NcmContentInstallType_Full;
    bool has = false;
    if (R_FAILED(ncmContentMetaDatabaseHas(&s->db, &has, &key)) || !has) {
        g->meta_missing = true;
        return;
    }
    u32 required = 0;
    if (R_SUCCEEDED(ncmContentMetaDatabaseGetRequiredSystemVersion(&s->db, &required, &key))) {
        const u32 hos = gamecheck_sysver_to_hos(required);
        if (hos > g->required_hos) g->required_hos = hos;
    }
    NcmContentInfo infos[CONTENT_CHUNK];
    for (s32 start = 0;;) {
        s32 got = 0;
        if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&s->db, &got, infos, CONTENT_CHUNK, &key, start)) || got <= 0) break;
        for (s32 i = 0; i < got; i++) {
            bool present = false;
            if (R_FAILED(ncmContentStorageHas(&s->cs, &present, &infos[i].content_id)) || !present) g->files_missing = true;
        }
        start += got;
        if (got < CONTENT_CHUNK) break;
    }
}

static void check_game(GameFacts *g, NsApplicationControlData *cd, Storage *storages, size_t n)
{
    u64 size = 0;
    NacpLanguageEntry *lang = NULL;
    if (cd && R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, g->app_id, cd, sizeof(*cd), &size)) &&
        size > sizeof(cd->nacp) && R_SUCCEEDED(nacpGetLanguageEntry(&cd->nacp, &lang)) && lang && lang->name[0]) {
        g->control_ok = true;
        copy_utf8(g->name, sizeof(g->name), lang->name);
    }

    NsApplicationContentMetaStatus metas[META_CHUNK];
    bool all_card = true;
    for (s32 index = 0;;) {
        s32 got = 0;
        if (R_FAILED(nsListApplicationContentMetaStatus(g->app_id, index, metas, META_CHUNK, &got)) || got <= 0) break;
        for (s32 i = 0; i < got; i++) {
            const NsApplicationContentMetaStatus *m = &metas[i];
            g->meta_count++;
            if (m->storageID != NcmStorageId_GameCard) all_card = false;
            const bool base  = m->meta_type == NcmContentMetaType_Application;
            const bool patch = m->meta_type == NcmContentMetaType_Patch;
            if (base) {
                g->has_base = true;
                g->base_version = m->version;
            }
            if (patch) {
                g->has_patch = true;
                g->patch_version = m->version;
            }
            // A card may be out of its slot: only the console's memory and
            // the SD card are checked. Add-ons do not stop the game from
            // starting.
            if ((base || patch) && n && (m->storageID == NcmStorageId_BuiltInUser || m->storageID == NcmStorageId_SdCard))
                check_meta(g, m, storages, n);
        }
        index += got;
        if (got < META_CHUNK) break;
    }
    g->card_only = g->meta_count > 0 && all_card;
    g->launch_rc = nsCheckApplicationLaunchVersion(g->app_id);
}

void gamecheck_scan(GameCheck *out)
{
    memset(out, 0, sizeof(*out));
    out->rc = nsInitialize();
    if (R_FAILED(out->rc)) return;

    NsApplicationRecord records[RECORD_CHUNK];
    for (s32 offset = 0;;) {
        s32 got = 0;
        const Result rc = nsListApplicationRecord(records, RECORD_CHUNK, offset, &got);
        if (R_FAILED(rc)) {
            if (offset == 0) out->rc = rc;
            break;
        }
        if (got <= 0) break;
        for (s32 i = 0; i < got; i++) {
            if (out->count == GAMECHECK_MAX) {
                out->truncated = true;
                break;
            }
            out->games[out->count++].app_id = records[i].application_id;
        }
        offset += got;
        if (got < RECORD_CHUNK || out->truncated) break;
    }
    if (R_FAILED(out->rc)) {
        nsExit();
        return;
    }

    // Without ncm, only what ns says: the files are not checked.
    const bool ncm = R_SUCCEEDED(ncmInitialize());
    Storage storages[2];
    memset(storages, 0, sizeof(storages));
    storages[0].id = NcmStorageId_BuiltInUser;
    storages[1].id = NcmStorageId_SdCard;
    const size_t n = ncm ? sizeof(storages) / sizeof(storages[0]) : 0;
    NsApplicationControlData *cd = (NsApplicationControlData *)malloc(sizeof(NsApplicationControlData));
    for (u32 i = 0; i < out->count; i++) check_game(&out->games[i], cd, storages, n);
    free(cd);
    if (ncm) {
        storages_close(storages, n);
        ncmExit();
    }
    nsExit();
}

Result gamecheck_remove(u64 app_id)
{
    const Result gate = core_change_allowed();
    if (R_FAILED(gate)) return gate;
    Result rc = nsInitialize();
    if (R_FAILED(rc)) return rc;
    rc = nsDeleteApplicationCompletely(app_id);
    nsExit();
    return rc;
}
