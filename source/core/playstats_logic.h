// playstats_logic — the parts of the play data layer (playstats.c) that never
// talk to a service: one raw log entry as a playlog event, the bounded walk
// of the log through a query function, a game's name out of its control.nacp
// (titles compressed on 21.0.0+ included), the names already known, and the
// game icons kept on the SD card. libnx calls stay in playstats.c, so the
// host tests (tests/playstats/) run all of this against a fake <switch.h>.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "playstats.h"

// One raw log entry as a playlog event; false for entries that do not matter.
bool playstats_convert(const PdmPlayEvent *p, PlayLogEvent *e);

// Reads `count` raw entries from `index` (pdmqryQueryPlayEvent's signature).
typedef Result (*PlayEventQuery)(s32 index, PdmPlayEvent *events, s32 count, s32 *got);

#define PLAYSTATS_EVENT_CHUNK 256
// At most this many events are kept (48 bytes each): beyond it the clock was
// set back months and every entry since then looks recent (see below).
#define PLAYSTATS_EVENTS_MAX  65536

// The index to read the log from for entries since `since` (user clock), in
// the log [start, start + total): back from the newest entry a chunk at a
// time, until a whole chunk is older. `chunk` holds PLAYSTATS_EVENT_CHUNK.
s32 playstats_first_recent_index(PlayEventQuery query, s32 start, s32 total, u64 since, PdmPlayEvent *chunk);

// The log entries since `since`, oldest first, at most `max` of them: the
// newest ones when there are more, and *capped says so. *events is malloc'ed
// (NULL when empty); the caller frees it.
Result playstats_collect_events(PlayEventQuery query, s32 start, s32 total, u64 since, size_t max,
                                PlayLogEvent **events, size_t *count, bool *capped);

// `s` is valid UTF-8 for its first `len` bytes (no overlong form, surrogate
// or code point above U+10FFFF).
bool playstats_utf8_valid(const char *s, size_t len);
// Copies at most n-1 bytes of a UTF-8 string without cutting a character.
void playstats_copy_utf8(char *dst, size_t n, const char *src);

// Byte 0x3215 of a control.nacp ([21.0.0+] TitlesDataFormat): 1 when the
// title block (0x3000 bytes at 0) is {u16 size; raw DEFLATE} that inflates to
// up to 32 language entries of 0x300 bytes.
#define PLAYSTATS_NACP_SIZE          0x4000
#define PLAYSTATS_NACP_TITLES_FORMAT 0x3215

// The game's name from its control.nacp (`size` bytes, at least
// PLAYSTATS_NACP_SIZE): the entry of language `lang` (an index into the
// title entries), else the first one with a name or a publisher, as
// nacpGetLanguageEntry() picks it. False, `out` empty, when there is none or
// it is not valid UTF-8 (the UI then names the game by its title ID).
bool playstats_nacp_name(const void *nacp, size_t size, u32 lang, char *out, size_t out_size);

// The names already read: this run's, and the last run's from the SD cache
// (PlayStats.names_lang says in which console language they are).
typedef struct {
    u8  lang;    // PlayStats.names_lang of every name held, 0 unknown
    u32 count;
    struct { u64 id; char name[PLAYSTATS_NAME_LEN]; } e[PLAYSTATS_MAX];
} PlayNames;

// The console language is `lang` now: names in another one are dropped.
void playnames_use_language(PlayNames *m, u8 lang);
// Adds the names of an earlier read (its language unknown: none).
void playnames_seed(PlayNames *m, const PlayStats *known);
const char *playnames_find(const PlayNames *m, u64 id);   // NULL when unknown
void playnames_add(PlayNames *m, u64 id, const char *name);

// Game icons on the SD card: one JPEG per game (<dir>/<title ID>.jpg) in the
// console language of a `lang` file next to them. Opening it with another
// language, or a folder over its bounds (a crash between two runs), empties
// it; a run then adds at most `max_bytes` / `max_files` in all.
typedef struct {
    char dir[192];
    u8   lang;
    bool ready;
    u64  bytes, max_bytes;
    u32  files, max_files;
} IconStore;

bool icon_store_open(IconStore *s, const char *dir, u8 lang, u64 max_bytes, u32 max_files);
// The JPEG kept for `id`, malloc'ed (the caller frees it), or NULL.
unsigned char *icon_store_get(const IconStore *s, u64 id, size_t *size);
void icon_store_put(IconStore *s, u64 id, const unsigned char *jpeg, size_t size);
