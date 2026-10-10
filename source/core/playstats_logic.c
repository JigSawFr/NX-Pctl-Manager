// PlayGuard — the service-free part of the play data layer (see playstats_logic.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "playstats_logic.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

bool playstats_convert(const PdmPlayEvent *p, PlayLogEvent *e)
{
    e->app_id    = 0;
    e->ts_user   = p->timestamp_user;
    e->ts_steady = p->timestamp_steady;
    e->uid[0] = e->uid[1] = 0;

    if (p->play_event_type == PdmPlayEventType_Account) {
        // 0: the account is opened (picked in the game), 1: closed. The u32
        // halves are stored swapped in each u64, as for the ProgramId below.
        const u8 type = p->event_data.account.type;
        if (type > 1) return false;
        const u32 *w = p->event_data.account.uid;
        e->uid[0] = ((u64)w[0] << 32) | w[1];
        e->uid[1] = ((u64)w[2] << 32) | w[3];
        e->kind   = type == 0 ? PlayLogEv_AccountOpen : PlayLogEv_AccountClose;
        return true;
    }
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
        case PdmAppletEventType_Launch:   // a new start: whatever was in focus is over,
            e->app_id = 0;                // and the accounts the previous game had open
            e->kind   = PlayLogEv_Launch;
            return true;
        default:
            return false;
    }
}

// A whole chunk, not the first old entry: a clock set back makes the user
// times go back and forth. Only the last week matters; the log holds years.
s32 playstats_first_recent_index(PlayEventQuery query, s32 start, s32 total, u64 since, PdmPlayEvent *chunk)
{
    s32 hi = start + total;   // one past the newest entry
    while (hi > start) {
        const s32 lo = hi - PLAYSTATS_EVENT_CHUNK > start ? hi - PLAYSTATS_EVENT_CHUNK : start;
        s32 got = 0;
        if (R_FAILED(query(lo, chunk, hi - lo, &got)) || got <= 0) return start;   // all of it, then
        bool recent = false;
        for (s32 i = 0; i < got && !recent; i++) recent = chunk[i].timestamp_user >= since;
        if (!recent) return hi;
        hi = lo;
    }
    return start;
}

Result playstats_collect_events(PlayEventQuery query, s32 start, s32 total, u64 since, size_t max,
                                PlayLogEvent **events, size_t *count, bool *capped)
{
    *events = NULL;
    *count  = 0;
    *capped = false;
    if (total <= 0 || max == 0) return 0;
    PdmPlayEvent *chunk = (PdmPlayEvent *)malloc(sizeof(PdmPlayEvent) * PLAYSTATS_EVENT_CHUNK);
    if (!chunk) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    s32 index = playstats_first_recent_index(query, start, total, since, chunk);
    // The clock set back months: every entry written since looks recent.
    // Only the newest, so the read stays bounded (and today stays right).
    if ((u64)(start + total - index) > max) {
        index   = (s32)((u64)start + (u64)total - max);
        *capped = true;
    }
    s32 remaining = start + total - index;
    size_t cap = 0;
    Result rc = 0;
    while (remaining > 0 && R_SUCCEEDED(rc)) {
        s32 got = 0;
        rc = query(index, chunk, remaining < PLAYSTATS_EVENT_CHUNK ? remaining : PLAYSTATS_EVENT_CHUNK, &got);
        if (R_FAILED(rc) || got <= 0) break;
        index += got;
        remaining -= got;
        for (s32 i = 0; i < got; i++) {
            PlayLogEvent e;
            if (!playstats_convert(&chunk[i], &e) || e.ts_user < since) continue;
            if (*count == max) {   // more than asked for after all (the log grew meanwhile)
                *capped   = true;
                remaining = 0;
                break;
            }
            if (*count == cap) {
                size_t grown = cap ? cap * 2 : 1024;
                if (grown > max) grown = max;
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

bool playstats_utf8_valid(const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < len;) {
        const unsigned char c = p[i];
        size_t n;
        u32 cp;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 0x07; }
        else return false;   // a continuation byte first, an overlong 0xC0 / 0xC1, or past U+10FFFF
        if (len - i <= n) return false;
        for (size_t k = 1; k <= n; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        if ((n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += n + 1;
    }
    return true;
}

void playstats_copy_utf8(char *dst, size_t n, const char *src)
{
    size_t len = strnlen(src, n - 1);
    if (len == n - 1)
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80) len--;   // inside a character
    memcpy(dst, src, len);
    dst[len] = '\0';
}

#define TITLE_SIZE      0x300    // NacpLanguageEntry: name 0x200, publisher 0x100
#define TITLE_NAME      0x200
#define TITLES_PLAIN    16       // entries of the uncompressed block (0x3000 bytes)
#define TITLES_PACKED   32       // what a compressed block inflates to, at most
#define TITLES_BLOCK    0x3000

// Inflates a compressed title block into `out` (TITLES_PACKED entries);
// returns the number of whole entries, 0 when it is broken.
static u32 inflate_titles(const u8 *block, u8 *out)
{
    const u32 packed = (u32)block[0] | ((u32)block[1] << 8);   // little-endian
    if (packed == 0 || packed > TITLES_BLOCK - 2) return 0;
    z_stream z;
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -15) != Z_OK) return 0;   // raw DEFLATE, no zlib header
    z.next_in   = (Bytef *)(block + 2);
    z.avail_in  = packed;
    z.next_out  = out;
    z.avail_out = TITLES_PACKED * TITLE_SIZE;
    const int r = inflate(&z, Z_FINISH);
    const size_t made = TITLES_PACKED * TITLE_SIZE - z.avail_out;
    inflateEnd(&z);
    // The whole stream, or a full buffer (32 entries are all there can be).
    if (r != Z_STREAM_END && z.avail_out != 0) return 0;
    return (u32)(made / TITLE_SIZE);
}

static bool title_used(const u8 *t)
{
    return t[0] != 0 || t[TITLE_NAME] != 0;   // a name or a publisher
}

bool playstats_nacp_name(const void *nacp, size_t size, u32 lang, char *out, size_t out_size)
{
    out[0] = '\0';
    if (size < PLAYSTATS_NACP_SIZE) return false;
    const u8 *bytes = (const u8 *)nacp;
    const u8 *titles = bytes;
    u32 count = TITLES_PLAIN;
    u8 *inflated = NULL;
    switch (bytes[PLAYSTATS_NACP_TITLES_FORMAT]) {
        case 0:
            break;
        case 1:
            inflated = (u8 *)malloc(TITLES_PACKED * TITLE_SIZE);
            if (!inflated) return false;
            count  = inflate_titles(bytes, inflated);
            titles = inflated;
            break;
        default:   // a format this code does not know: no guess
            return false;
    }
    const u8 *t = lang < count ? titles + (size_t)lang * TITLE_SIZE : NULL;
    for (u32 i = 0; (!t || !title_used(t)) && i < count; i++) t = titles + (size_t)i * TITLE_SIZE;
    bool ok = false;
    if (t && title_used(t)) {
        const char *name = (const char *)t;
        const size_t len = strnlen(name, TITLE_NAME);
        if (len > 0 && len < TITLE_NAME && playstats_utf8_valid(name, len)) {
            playstats_copy_utf8(out, out_size, name);
            ok = out[0] != '\0';
        }
    }
    free(inflated);
    return ok;
}

void playnames_use_language(PlayNames *m, u8 lang)
{
    if (m->lang == lang) return;
    m->lang  = lang;
    m->count = 0;
}

const char *playnames_find(const PlayNames *m, u64 id)
{
    for (u32 i = 0; i < m->count; i++)
        if (m->e[i].id == id) return m->e[i].name;
    return NULL;
}

void playnames_add(PlayNames *m, u64 id, const char *name)
{
    if (!name[0] || playnames_find(m, id) || m->count == PLAYSTATS_MAX) return;
    m->e[m->count].id = id;
    playstats_copy_utf8(m->e[m->count].name, sizeof(m->e[m->count].name), name);
    m->count++;
}

void playnames_seed(PlayNames *m, const PlayStats *known)
{
    if (!known || !known->names_lang) return;
    if (m->lang == 0 && m->count == 0) m->lang = known->names_lang;   // the first read of the run
    if (m->lang != known->names_lang) return;   // this run's names, in the language of now
    for (u32 i = 0; i < known->count && i < PLAYSTATS_MAX; i++) {
        const GameStat *g = &known->games[i];
        if (playstats_utf8_valid(g->name, strnlen(g->name, sizeof(g->name)))) playnames_add(m, g->app_id, g->name);
    }
}

// ---------------------------------------------------------------- icon store

static bool icon_path(const IconStore *s, u64 id, const char *ext, char *path, size_t size)
{
    const int n = snprintf(path, size, "%s/%016llX%s", s->dir, (unsigned long long)id, ext);
    return n > 0 && (size_t)n < size;
}

// "<16 hex digits><ext>", as icon_path() names them.
static bool is_icon_file(const char *name, const char *ext)
{
    return strlen(name) == 16 + 4 && strcmp(name + 16, ext) == 0;
}

// Adds up the icons of the folder (with `remove_all`, deletes them); a .tmp
// left by a crash goes either way. Returns how many files were deleted.
static u32 scan_once(IconStore *s, bool remove_all)
{
    s->bytes = 0;
    s->files = 0;
    DIR *d = opendir(s->dir);
    if (!d) return 0;
    char path[sizeof(s->dir) + 32];
    u32 removed = 0;
    for (struct dirent *e; (e = readdir(d));) {
        const bool icon = is_icon_file(e->d_name, ".jpg");
        if (!icon && !is_icon_file(e->d_name, ".tmp")) continue;
        if (snprintf(path, sizeof(path), "%s/%s", s->dir, e->d_name) >= (int)sizeof(path)) continue;
        struct stat st;
        if (remove_all || !icon) {
            if (remove(path) == 0) removed++;
        } else if (stat(path, &st) == 0) {
            s->bytes += (u64)st.st_size;
            s->files++;
        }
    }
    closedir(d);
    return removed;
}

static void scan(IconStore *s, bool remove_all)
{
    // Again while it deletes: a listing may skip entries deleted under it.
    for (int pass = 0; pass < 4 && scan_once(s, remove_all) > 0; pass++) {}
}

// mkdir -p, for a path with no "." or ".." parts.
static bool make_dirs(char *path)
{
    for (char *p = path + 1;; p++) {
        if (*p != '/' && *p != '\0') continue;
        const char c = *p;
        *p = '\0';
        const bool ok = mkdir(path, 0777) == 0 || errno == EEXIST;
        *p = c;
        if (!ok) return false;
        if (c == '\0') return true;
    }
}

bool icon_store_open(IconStore *s, const char *dir, u8 lang, u64 max_bytes, u32 max_files)
{
    memset(s, 0, sizeof(*s));
    if (!dir || !dir[0] || strlen(dir) >= sizeof(s->dir)) return false;
    strcpy(s->dir, dir);
    if (!make_dirs(s->dir)) return false;
    s->lang      = lang;
    s->max_bytes = max_bytes;
    s->max_files = max_files;
    char path[sizeof(s->dir) + 8];
    snprintf(path, sizeof(path), "%s/lang", s->dir);
    int stored = -1;
    FILE *f = fopen(path, "rb");
    if (f) {
        stored = fgetc(f);
        fclose(f);
    }
    if (stored == (int)lang) scan(s, false);
    // Another language, or full: start again with the games played now.
    if (stored != (int)lang || s->bytes >= max_bytes || s->files >= max_files) {
        scan(s, true);
        s->bytes = 0;
        s->files = 0;
        f = fopen(path, "wb");
        if (!f) return false;
        const bool written = fputc(lang, f) != EOF;
        if (fclose(f) != 0 || !written) return false;
    }
    s->ready = true;
    return true;
}

static bool looks_like_jpeg(const unsigned char *p, size_t size)
{
    return size >= 4 && size <= PLAYSTATS_ICON_MAX && p[0] == 0xFF && p[1] == 0xD8;
}

unsigned char *icon_store_get(const IconStore *s, u64 id, size_t *size)
{
    *size = 0;
    char path[sizeof(s->dir) + 32];
    if (!s->ready || !icon_path(s, id, ".jpg", path, sizeof(path))) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char *buf = NULL;
    long len = -1;
    if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && len <= PLAYSTATS_ICON_MAX && fseek(f, 0, SEEK_SET) == 0)
        buf = (unsigned char *)malloc((size_t)len);
    if (buf && fread(buf, 1, (size_t)len, f) == (size_t)len && looks_like_jpeg(buf, (size_t)len)) {
        *size = (size_t)len;
    } else {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

void icon_store_put(IconStore *s, u64 id, const unsigned char *jpeg, size_t size)
{
    char tmp[sizeof(s->dir) + 32], path[sizeof(s->dir) + 32];
    if (!s->ready || !looks_like_jpeg(jpeg, size) || s->files >= s->max_files || s->bytes + size > s->max_bytes ||
        !icon_path(s, id, ".tmp", tmp, sizeof(tmp)) || !icon_path(s, id, ".jpg", path, sizeof(path)))
        return;
    // Written aside, then renamed: a file cut by a crash is never read as an icon.
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    const bool written = fwrite(jpeg, 1, size, f) == size;
    if (fclose(f) != 0 || !written) {
        remove(tmp);
        return;
    }
    remove(path);   // the console's rename does not replace a file
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return;
    }
    s->bytes += size;
    s->files++;
}
