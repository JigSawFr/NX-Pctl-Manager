// Host tests for source/core/playstats_logic.c: raw log entries as playlog
// events, the bounded walk of the log, game names out of a control.nacp
// (compressed titles included), the names kept between reads, and the icons
// kept on the SD card.
#define _DEFAULT_SOURCE   // mkdtemp
#include "check.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "playstats_logic.h"

// A file for the test to damage, owner-writable only (CodeQL: no 0666).
static FILE *create_file(const char *path)
{
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return fd < 0 ? NULL : fdopen(fd, "wb");
}

// ---------------------------------------------------------------- convert

static PdmPlayEvent applet(u8 applet_id, u8 type, u64 program, u64 ts)
{
    PdmPlayEvent p;
    memset(&p, 0, sizeof(p));
    p.play_event_type = PdmPlayEventType_Applet;
    p.event_data.applet.applet_id = applet_id;
    p.event_data.applet.event_type = type;
    p.event_data.applet.log_policy = PdmPlayLogPolicy_All;
    p.event_data.applet.program_id[0] = (u32)(program >> 32);   // stored swapped
    p.event_data.applet.program_id[1] = (u32)program;
    p.timestamp_user = ts;
    p.timestamp_steady = ts + 7;
    return p;
}

static void test_convert(void)
{
    const u64 GAME = 0x0100A1B2C3D40000ULL;
    PlayLogEvent e;
    PdmPlayEvent p = applet(AppletId_application, PdmAppletEventType_InFocus, GAME, 1000);
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_Focus && e.app_id == GAME);
    CHECK(e.ts_user == 1000 && e.ts_steady == 1007 && e.uid[0] == 0 && e.uid[1] == 0);

    const u8 unfocus[] = { PdmAppletEventType_OutOfFocus, PdmAppletEventType_OutOfFocus4, PdmAppletEventType_Exit,
                           PdmAppletEventType_Exit5, PdmAppletEventType_Exit6 };
    for (size_t i = 0; i < sizeof(unfocus); i++) {
        p = applet(AppletId_application, unfocus[i], GAME, 1000);
        CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_Unfocus && e.app_id == GAME);
    }
    // A launch ends what was in focus, whatever game it is.
    p = applet(AppletId_application, PdmAppletEventType_Launch, GAME, 1000);
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_Launch && e.app_id == 0);
    p = applet(AppletId_application, 9, GAME, 1000);
    CHECK(!playstats_convert(&p, &e));
    // Left out of the system's statistics: left out here too.
    p = applet(AppletId_application, PdmAppletEventType_InFocus, GAME, 1000);
    p.event_data.applet.log_policy = PdmPlayLogPolicy_LogOnly;
    CHECK(!playstats_convert(&p, &e));
    // The HOME menu in focus: nothing is played; other applets do not matter.
    p = applet(AppletId_SystemAppletMenu, PdmAppletEventType_InFocus, 0x0100000000001000ULL, 1000);
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_Away && e.app_id == 0);
    p = applet(AppletId_SystemAppletMenu, PdmAppletEventType_OutOfFocus, 0x0100000000001000ULL, 1000);
    CHECK(!playstats_convert(&p, &e));
    p = applet(0x13, PdmAppletEventType_InFocus, 0x0100000000001001ULL, 1000);
    CHECK(!playstats_convert(&p, &e));

    // Accounts: opened 0, closed 1; the u32 halves swapped in each u64.
    memset(&p, 0, sizeof(p));
    p.play_event_type = PdmPlayEventType_Account;
    p.event_data.account.uid[0] = 0x11111111;
    p.event_data.account.uid[1] = 0x22222222;
    p.event_data.account.uid[2] = 0x33333333;
    p.event_data.account.uid[3] = 0x44444444;
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_AccountOpen);
    CHECK(e.uid[0] == 0x1111111122222222ULL && e.uid[1] == 0x3333333344444444ULL);
    p.event_data.account.type = 1;
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_AccountClose);
    p.event_data.account.type = 2;
    CHECK(!playstats_convert(&p, &e));

    memset(&p, 0, sizeof(p));
    p.play_event_type = PdmPlayEventType_PowerStateChange;
    CHECK(playstats_convert(&p, &e) && e.kind == PlayLogEv_Away);
    p.play_event_type = PdmPlayEventType_OperationModeChange;
    CHECK(!playstats_convert(&p, &e));
}

// ---------------------------------------------------------------- the log

// A fake log: entries [LOG_START, LOG_START + log_n), every one a focus.
#define LOG_START 5000
static PdmPlayEvent *s_log;
static s32 s_log_n;
static int s_queries;
static int s_fail_after = -1;   // fail from the n-th query on

static Result fake_query(s32 index, PdmPlayEvent *events, s32 count, s32 *got)
{
    *got = 0;
    if (s_fail_after >= 0 && s_queries >= s_fail_after) return 1;
    s_queries++;
    REQUIRE(count > 0 && count <= PLAYSTATS_EVENT_CHUNK);
    for (s32 i = 0; i < count && index + i < LOG_START + s_log_n; i++) {
        REQUIRE(index + i >= LOG_START);
        events[i] = s_log[index + i - LOG_START];
        ++*got;
    }
    return 0;
}

// `n` entries, a minute apart from `t0`.
static void make_log(s32 n, u64 t0)
{
    free(s_log);
    s_log = (PdmPlayEvent *)calloc((size_t)n, sizeof(PdmPlayEvent));
    REQUIRE(s_log != NULL);
    s_log_n = n;
    for (s32 i = 0; i < n; i++)
        s_log[i] = applet(AppletId_application, PdmAppletEventType_InFocus, 0x0100000000010000ULL + (u64)i, t0 + 60 * (u64)i);
    s_queries = 0;
    s_fail_after = -1;
}

static void test_log(void)
{
    PdmPlayEvent chunk[PLAYSTATS_EVENT_CHUNK];
    const u64 T0 = 1000000;

    // 2000 entries, the last 100 since `since`: the walk stops at the first
    // whole chunk with nothing recent, reading back a few chunks only.
    make_log(2000, T0);
    const u64 since = T0 + 60 * 1900;
    s32 from = playstats_first_recent_index(fake_query, LOG_START, 2000, since, chunk);
    CHECK(from == LOG_START + 2000 - PLAYSTATS_EVENT_CHUNK);   // the newest chunk has them, the one before not
    CHECK(s_queries == 2);

    PlayLogEvent *events = NULL;
    size_t count = 0;
    bool capped = true;
    CHECK(playstats_collect_events(fake_query, LOG_START, 2000, since, PLAYSTATS_EVENTS_MAX, &events, &count, &capped) == 0);
    CHECK(count == 100 && !capped && events);
    CHECK(events && events[0].ts_user == since && events[99].ts_user == T0 + 60 * 1999);
    free(events);

    // A clock set back: every entry looks recent, the walk goes to the start.
    from = playstats_first_recent_index(fake_query, LOG_START, 2000, 0, chunk);
    CHECK(from == LOG_START);
    // Capped: only the newest `max` entries are read, and the read says so.
    s_queries = 0;
    CHECK(playstats_collect_events(fake_query, LOG_START, 2000, 0, 300, &events, &count, &capped) == 0);
    CHECK(count == 300 && capped);
    CHECK(events && events[0].ts_user == T0 + 60 * 1700 && events[299].ts_user == T0 + 60 * 1999);
    free(events);

    // A query that fails while walking back: all of it, then (bounded by the cap).
    s_queries = 0;
    s_fail_after = 0;
    CHECK(playstats_first_recent_index(fake_query, LOG_START, 2000, since, chunk) == LOG_START);
    // A failing read (a quitting app makes every query fail) stops at once.
    s_queries = 0;
    s_fail_after = 3;
    CHECK(playstats_collect_events(fake_query, LOG_START, 2000, 0, PLAYSTATS_EVENTS_MAX, &events, &count, &capped) != 0);
    CHECK(s_queries == 3);
    free(events);

    // An empty log, or nothing wanted.
    CHECK(playstats_collect_events(fake_query, LOG_START, 0, 0, 10, &events, &count, &capped) == 0 && !events && !count);
    CHECK(playstats_collect_events(fake_query, LOG_START, 2000, 0, 0, &events, &count, &capped) == 0 && !events && !count);
    free(s_log);
    s_log = NULL;
}

// ---------------------------------------------------------------- names

static void test_utf8(void)
{
    CHECK(playstats_utf8_valid("Pixel Quest", 11));
    CHECK(playstats_utf8_valid("Pok\xC3\xA9mon \xE3\x83\x9D \xF0\x9F\x8E\xAE", 17));
    CHECK(playstats_utf8_valid("", 0));
    CHECK(!playstats_utf8_valid("\x80", 1));              // a continuation byte first
    CHECK(!playstats_utf8_valid("\xC0\x80", 2));          // overlong NUL
    CHECK(!playstats_utf8_valid("\xE0\x80\xAF", 3));      // overlong '/'
    CHECK(!playstats_utf8_valid("\xED\xA0\x80", 3));      // a surrogate
    CHECK(!playstats_utf8_valid("\xF4\x90\x80\x80", 4));  // past U+10FFFF
    CHECK(!playstats_utf8_valid("\xC3", 1));              // cut
    CHECK(!playstats_utf8_valid("\xE3\x83", 2));
    CHECK(!playstats_utf8_valid("a\xC3(", 3));

    char out[8];
    playstats_copy_utf8(out, sizeof(out), "abcd\xC3\xA9\xC3\xA9");   // 7 bytes fit, not half an é
    CHECK(strcmp(out, "abcd\xC3\xA9") == 0);
}

#define ENTRY 0x300

static void set_title(u8 *titles, int i, const char *name, const char *publisher)
{
    memset(titles + i * ENTRY, 0, ENTRY);
    memcpy(titles + i * ENTRY, name, strlen(name));
    memcpy(titles + i * ENTRY + 0x200, publisher, strlen(publisher));
}

// A control.nacp whose title block is `entries` compressed (format 1).
static void pack_titles(u8 *nacp, const u8 *entries, size_t size)
{
    memset(nacp, 0, 0x3000);
    z_stream z;
    memset(&z, 0, sizeof(z));
    REQUIRE(deflateInit2(&z, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    z.next_in = (Bytef *)entries;
    z.avail_in = (uInt)size;
    z.next_out = nacp + 2;
    z.avail_out = 0x3000 - 2;
    REQUIRE(deflate(&z, Z_FINISH) == Z_STREAM_END);
    const size_t packed = 0x3000 - 2 - z.avail_out;
    deflateEnd(&z);
    nacp[0] = (u8)packed;
    nacp[1] = (u8)(packed >> 8);
    nacp[PLAYSTATS_NACP_TITLES_FORMAT] = 1;
}

static void test_nacp(void)
{
    u8 *nacp = (u8 *)calloc(1, PLAYSTATS_NACP_SIZE);
    u8 *big = (u8 *)calloc(32, ENTRY);
    REQUIRE(nacp && big);
    char name[PLAYSTATS_NAME_LEN];

    // Format 0: the entry of the language, else the first one used.
    set_title(nacp, 0, "Star Kart Racers", "Nin");
    set_title(nacp, 3, "Star Kart Racers (FR)", "Nin");
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 3, name, sizeof(name)) && strcmp(name, "Star Kart Racers (FR)") == 0);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 5, name, sizeof(name)) && strcmp(name, "Star Kart Racers") == 0);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 99, name, sizeof(name)) && strcmp(name, "Star Kart Racers") == 0);
    set_title(nacp, 0, "", "");
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 1, name, sizeof(name)) && strcmp(name, "Star Kart Racers (FR)") == 0);
    // A long name is cut on a whole character.
    char longer[300];
    memset(longer, 'a', 126);
    strcpy(longer + 126, "\xC3\xA9xyz");
    set_title(nacp, 3, longer, "");
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 3, name, sizeof(name)) && strlen(name) == 126);
    // Garbage, a publisher with no name, no title, a short read: no name.
    set_title(nacp, 3, "Bad \xFF name", "");
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 3, name, sizeof(name)) && name[0] == '\0');
    set_title(nacp, 3, "", "Publisher only");
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 3, name, sizeof(name)));
    memset(nacp, 0, PLAYSTATS_NACP_SIZE);
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 0, name, sizeof(name)));
    set_title(nacp, 0, "Island Builders", "");
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE - 1, 0, name, sizeof(name)));
    memset(nacp, 'x', 0x200);   // a name with no end
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 0, name, sizeof(name)));

    // Format 1 (21.0.0+): 32 entries compressed; one past the first 16 too.
    set_title(big, 0, "Dragon Valley Legends", "Pub");
    set_title(big, 12, "\xEB\x93\x9C\xEB\x9E\x98\xEA\xB3\xA4", "Pub");
    set_title(big, 20, "Entry twenty", "Pub");
    pack_titles(nacp, big, 32 * ENTRY);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 12, name, sizeof(name)) && strcmp(name, "\xEB\x93\x9C\xEB\x9E\x98\xEA\xB3\xA4") == 0);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 20, name, sizeof(name)) && strcmp(name, "Entry twenty") == 0);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 3, name, sizeof(name)) && strcmp(name, "Dragon Valley Legends") == 0);
    // The deflate bytes are never read as a name (what nacpGetLanguageEntry gives).
    CHECK(strcmp(name, (const char *)nacp + 2) != 0);
    // 16 entries compressed are fine too.
    set_title(big, 0, "", "");
    set_title(big, 7, "Sixteen", "Pub");
    pack_titles(nacp, big, 16 * ENTRY);
    CHECK(playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 0, name, sizeof(name)) && strcmp(name, "Sixteen") == 0);
    // Invalid UTF-8 inside: no name rather than garbage.
    set_title(big, 7, "Bad \xC0\xAF", "Pub");
    pack_titles(nacp, big, 16 * ENTRY);
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 7, name, sizeof(name)));

    // A broken block: a cut stream, a size of 0 or past the block, garbage.
    set_title(big, 7, "Sixteen", "Pub");
    pack_titles(nacp, big, 16 * ENTRY);
    const u32 packed = (u32)nacp[0] | ((u32)nacp[1] << 8);
    nacp[0] = (u8)(packed / 2);
    nacp[1] = (u8)((packed / 2) >> 8);
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 7, name, sizeof(name)));
    nacp[0] = nacp[1] = 0;
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 7, name, sizeof(name)));
    nacp[0] = 0xFF;
    nacp[1] = 0x2F;   // 0x2FFF: one past the room there is
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 7, name, sizeof(name)));
    memset(nacp + 2, 0xA5, 0x100);
    nacp[0] = 0x00;
    nacp[1] = 0x01;
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 7, name, sizeof(name)));
    // A format this code does not know.
    set_title(nacp, 0, "Island Builders", "");
    nacp[PLAYSTATS_NACP_TITLES_FORMAT] = 2;
    CHECK(!playstats_nacp_name(nacp, PLAYSTATS_NACP_SIZE, 0, name, sizeof(name)));
    free(big);
    free(nacp);
}

static void test_names(void)
{
    static PlayNames m;
    static PlayStats known;
    memset(&m, 0, sizeof(m));
    memset(&known, 0, sizeof(known));
    known.count = 3;
    known.games[0].app_id = 1;
    strcpy(known.games[0].name, "One");
    known.games[1].app_id = 2;   // deleted: no name
    known.games[2].app_id = 3;
    strcpy(known.games[2].name, "Bad \xFF");

    // An older cache, its language unknown: nothing taken.
    playnames_seed(&m, &known);
    playnames_seed(&m, NULL);
    CHECK(m.count == 0 && !playnames_find(&m, 1));

    // In French: taken while the console is in French.
    known.names_lang = 3;
    playnames_seed(&m, &known);
    CHECK(m.lang == 3 && m.count == 1 && strcmp(playnames_find(&m, 1), "One") == 0);
    CHECK(!playnames_find(&m, 2) && !playnames_find(&m, 3));
    playnames_use_language(&m, 3);
    CHECK(playnames_find(&m, 1) != NULL);
    playnames_add(&m, 4, "Four");
    playnames_add(&m, 4, "Four again");
    playnames_add(&m, 5, "");
    CHECK(m.count == 2 && strcmp(playnames_find(&m, 4), "Four") == 0 && !playnames_find(&m, 5));

    // The console language changed: every name is read again, and a cache
    // in the old one adds nothing.
    playnames_use_language(&m, 4);
    CHECK(m.lang == 4 && m.count == 0 && !playnames_find(&m, 1));
    playnames_seed(&m, &known);
    CHECK(m.count == 0);
    known.names_lang = 4;
    playnames_seed(&m, &known);
    CHECK(m.count == 1 && playnames_find(&m, 1));

    // Never more than PLAYSTATS_MAX.
    for (u64 id = 100; id < 100 + 2 * PLAYSTATS_MAX; id++) playnames_add(&m, id, "Game");
    CHECK(m.count == PLAYSTATS_MAX && playnames_find(&m, 1));
}

// ---------------------------------------------------------------- icons

static size_t files_in(const char *dir)
{
    size_t n = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    for (struct dirent *e; (e = readdir(d));) n += e->d_name[0] != '.';
    closedir(d);
    return n;
}

static void test_icons(void)
{
    char root[] = "/tmp/playguard_playstats_XXXXXX";
    REQUIRE(mkdtemp(root) != NULL);
    char dir[128];
    snprintf(dir, sizeof(dir), "%s/cache/icons", root);
    unsigned char jpeg[1000];
    memset(jpeg, 0x42, sizeof(jpeg));
    jpeg[0] = 0xFF;
    jpeg[1] = 0xD8;

    IconStore s;
    CHECK(icon_store_open(&s, dir, 3, 4000, 8));   // makes the folders
    size_t size = 1;
    CHECK(icon_store_get(&s, 0x0100A1B2C3D40000ULL, &size) == NULL && size == 0);
    icon_store_put(&s, 0x0100A1B2C3D40000ULL, jpeg, sizeof(jpeg));
    unsigned char *got = icon_store_get(&s, 0x0100A1B2C3D40000ULL, &size);
    CHECK(got && size == sizeof(jpeg) && memcmp(got, jpeg, size) == 0);
    free(got);
    char path[192];
    snprintf(path, sizeof(path), "%s/0100A1B2C3D40000.jpg", dir);
    CHECK(access(path, F_OK) == 0);

    // Not a JPEG, or too large: not kept. Over the bytes allowed: not kept.
    unsigned char text[16] = "not a jpeg";
    icon_store_put(&s, 2, text, sizeof(text));
    CHECK(icon_store_get(&s, 2, &size) == NULL);
    icon_store_put(&s, 3, jpeg, sizeof(jpeg));
    icon_store_put(&s, 4, jpeg, sizeof(jpeg));
    icon_store_put(&s, 5, jpeg, sizeof(jpeg));   // 4000 bytes now
    icon_store_put(&s, 6, jpeg, sizeof(jpeg));
    CHECK(s.bytes == 4000 && icon_store_get(&s, 6, &size) == NULL);

    // A file damaged meanwhile is not handed out; a .tmp left by a crash goes.
    snprintf(path, sizeof(path), "%s/0000000000000003.jpg", dir);
    FILE *f = create_file(path);
    REQUIRE(f != NULL);
    fputs("junk", f);
    fclose(f);
    CHECK(icon_store_get(&s, 3, &size) == NULL);
    snprintf(path, sizeof(path), "%s/0000000000000009.tmp", dir);
    f = create_file(path);
    REQUIRE(f != NULL);
    fclose(f);

    // Opened again in the same language: kept, the .tmp gone.
    CHECK(icon_store_open(&s, dir, 3, 8000, 8));
    CHECK(s.files == 4 && s.bytes == 3004 && access(path, F_OK) != 0);
    got = icon_store_get(&s, 4, &size);
    CHECK(got && size == sizeof(jpeg));
    free(got);
    // Full (bytes or files) when opened: emptied, to fill again.
    CHECK(icon_store_open(&s, dir, 3, 3004, 8));
    CHECK(s.files == 0 && s.bytes == 0 && icon_store_get(&s, 4, &size) == NULL);
    icon_store_put(&s, 7, jpeg, sizeof(jpeg));
    icon_store_put(&s, 8, jpeg, sizeof(jpeg));
    CHECK(icon_store_open(&s, dir, 3, 1u << 20, 2));
    CHECK(s.files == 0 && icon_store_get(&s, 7, &size) == NULL);
    icon_store_put(&s, 7, jpeg, sizeof(jpeg));
    icon_store_put(&s, 8, jpeg, sizeof(jpeg));
    icon_store_put(&s, 9, jpeg, sizeof(jpeg));   // past max_files
    CHECK(s.files == 2 && icon_store_get(&s, 9, &size) == NULL);

    // Another console language: emptied (icons follow the language).
    CHECK(icon_store_open(&s, dir, 4, 1u << 20, 8));
    CHECK(s.files == 0 && icon_store_get(&s, 7, &size) == NULL);
    CHECK(files_in(dir) == 1);   // only the language file

    // Not opened, or no folder: nothing, quietly.
    IconStore none;
    memset(&none, 0, sizeof(none));
    icon_store_put(&none, 1, jpeg, sizeof(jpeg));
    CHECK(icon_store_get(&none, 1, &size) == NULL);
    CHECK(!icon_store_open(&none, "", 3, 4000, 8) && !none.ready);

    // Clean up.
    snprintf(path, sizeof(path), "%s/lang", dir);
    remove(path);
    rmdir(dir);
    snprintf(path, sizeof(path), "%s/cache", root);
    rmdir(path);
    rmdir(root);
}

int main(void)
{
    test_convert();
    test_log();
    test_utf8();
    test_nacp();
    test_names();
    test_icons();
    return CHECK_DONE("playstats convert, log, NACP name, name and icon cache assertions passed");
}
