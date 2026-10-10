// Host tests for source/core/gamecheck.c and gamecheck_rules.c: what the scan
// reads of each game from a scripted console (ns, ncm) and the problem it
// becomes, every session closed, paging past one chunk of games, ncm
// missing, a storage that cannot be opened; the removal's gates (read-only,
// the PIN asked before a change); the system-version conversion.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamecheck.h"
#include "write_guard.h"

// ------------------------------------------------------------ the console

#define MAX_GAMES 700
#define MAX_METAS 8
#define MAX_FILES 4

typedef struct {
    u8  type;
    u8  storage;
    u32 version;
    u64 id;
    bool registered;          // known to the storage's meta database
    u32 required_sysver;      // RequiredSystemVersion
    int files;                // contents listed for it
    int files_present;        // how many of them the storage has
} FakeMeta;

typedef struct {
    u64 id;
    const char *name;         // NULL: no control data
    int meta_count;
    FakeMeta metas[MAX_METAS];
    Result launch_rc;
} FakeGame;

static struct {
    FakeGame games[MAX_GAMES];
    int count;
    Result ns_init_rc, list_rc, ncm_init_rc;
    bool sd_unopenable;
    int ns_open, ncm_open, db_open, cs_open;   // sessions open right now
    int ns_inits;
    u64 deleted;
    Result delete_rc;
} C;

static void reset(void)
{
    memset(&C, 0, sizeof(C));
}

static FakeGame *add_game(u64 id, const char *name)
{
    FakeGame *g = &C.games[C.count++];
    memset(g, 0, sizeof(*g));
    g->id = id;
    g->name = name;
    return g;
}

static FakeMeta *add_meta(FakeGame *g, u8 type, u8 storage, u32 version)
{
    FakeMeta *m = &g->metas[g->meta_count++];
    m->type = type;
    m->storage = storage;
    m->version = version;
    m->id = type == NcmContentMetaType_Patch ? g->id | 0x800 : g->id;
    m->registered = true;
    m->files = m->files_present = 2;
    return m;
}

static FakeGame *find(u64 id)
{
    for (int i = 0; i < C.count; i++)
        if (C.games[i].id == id) return &C.games[i];
    return NULL;
}

// The meta a key names, on a storage.
static FakeMeta *meta_of(NcmStorageId storage, const NcmContentMetaKey *key)
{
    for (int i = 0; i < C.count; i++)
        for (int j = 0; j < C.games[i].meta_count; j++) {
            FakeMeta *m = &C.games[i].metas[j];
            if (m->storage == storage && m->id == key->id && m->version == key->version && m->type == key->type &&
                key->install_type == NcmContentInstallType_Full)
                return m;
        }
    return NULL;
}

Result nsInitialize(void)
{
    C.ns_inits++;
    if (C.ns_init_rc) return C.ns_init_rc;
    C.ns_open++;
    return 0;
}

void nsExit(void)
{
    assert(C.ns_open > 0);
    C.ns_open--;
}

Result nsListApplicationRecord(NsApplicationRecord *records, s32 count, s32 entry_offset, s32 *out)
{
    assert(C.ns_open == 1);
    if (C.list_rc) return C.list_rc;
    s32 n = 0;
    for (s32 i = entry_offset; i < C.count && n < count; i++, n++) {
        memset(&records[n], 0, sizeof(records[n]));
        records[n].application_id = C.games[i].id;
    }
    *out = n;
    return 0;
}

Result nsGetApplicationControlData(NsApplicationControlSource source, u64 id, NsApplicationControlData *buffer, size_t size,
                                   u64 *actual)
{
    assert(C.ns_open == 1 && source == NsApplicationControlSource_Storage && size == sizeof(*buffer));
    const FakeGame *g = find(id);
    if (!g || !g->name) return 0x1234;
    memset(&buffer->nacp, 0, sizeof(buffer->nacp));
    snprintf(buffer->nacp.lang[0].name, sizeof(buffer->nacp.lang[0].name), "%s", g->name);
    *actual = sizeof(buffer->nacp) + 0x1000;
    return 0;
}

Result nacpGetLanguageEntry(NacpStruct *nacp, NacpLanguageEntry **entry)
{
    *entry = &nacp->lang[0];
    return 0;
}

Result nsListApplicationContentMetaStatus(u64 id, s32 index, NsApplicationContentMetaStatus *list, s32 count, s32 *out)
{
    assert(C.ns_open == 1);
    const FakeGame *g = find(id);
    if (!g) return 0x2345;
    s32 n = 0;
    for (s32 i = index; i < g->meta_count && n < count; i++, n++) {
        memset(&list[n], 0, sizeof(list[n]));
        list[n].meta_type      = g->metas[i].type;
        list[n].storageID      = g->metas[i].storage;
        list[n].version        = g->metas[i].version;
        list[n].application_id = g->metas[i].id;
    }
    *out = n;
    return 0;
}

Result nsCheckApplicationLaunchVersion(u64 id)
{
    assert(C.ns_open == 1);
    const FakeGame *g = find(id);
    return g ? g->launch_rc : 0x3456;
}

Result nsDeleteApplicationCompletely(u64 id)
{
    assert(C.ns_open == 1);
    if (C.delete_rc) return C.delete_rc;
    C.deleted = id;
    return 0;
}

Result ncmInitialize(void)
{
    if (C.ncm_init_rc) return C.ncm_init_rc;
    C.ncm_open++;
    return 0;
}

void ncmExit(void)
{
    assert(C.ncm_open > 0 && C.db_open == 0 && C.cs_open == 0);   // the storages first
    C.ncm_open--;
}

static Result open_storage(NcmStorageId id)
{
    assert(C.ncm_open == 1);
    assert(id == NcmStorageId_BuiltInUser || id == NcmStorageId_SdCard);   // never the game card
    return id == NcmStorageId_SdCard && C.sd_unopenable ? 0x4567 : 0;
}

Result ncmOpenContentMetaDatabase(NcmContentMetaDatabase *out, NcmStorageId id)
{
    const Result rc = open_storage(id);
    if (rc) return rc;
    out->storage = id;
    out->open = true;
    C.db_open++;
    return 0;
}

Result ncmOpenContentStorage(NcmContentStorage *out, NcmStorageId id)
{
    const Result rc = open_storage(id);
    if (rc) return rc;
    out->storage = id;
    out->open = true;
    C.cs_open++;
    return 0;
}

void ncmContentMetaDatabaseClose(NcmContentMetaDatabase *db)
{
    assert(db->open);
    db->open = false;
    C.db_open--;
}

void ncmContentStorageClose(NcmContentStorage *cs)
{
    assert(cs->open);
    cs->open = false;
    C.cs_open--;
}

Result ncmContentMetaDatabaseHas(NcmContentMetaDatabase *db, bool *out, const NcmContentMetaKey *key)
{
    assert(db->open);
    const FakeMeta *m = meta_of(db->storage, key);
    *out = m && m->registered;
    return 0;
}

Result ncmContentMetaDatabaseGetRequiredSystemVersion(NcmContentMetaDatabase *db, u32 *out, const NcmContentMetaKey *key)
{
    assert(db->open);
    const FakeMeta *m = meta_of(db->storage, key);
    if (!m || !m->registered) return 0x5678;
    *out = m->required_sysver;
    return 0;
}

// Content IDs: byte 0 the meta's index in its game + 1, byte 1 the file.
Result ncmContentMetaDatabaseListContentInfo(NcmContentMetaDatabase *db, s32 *written, NcmContentInfo *info, s32 count,
                                             const NcmContentMetaKey *key, s32 start)
{
    assert(db->open);
    const FakeMeta *m = meta_of(db->storage, key);
    if (!m || !m->registered) return 0x5678;
    s32 n = 0;
    for (s32 i = start; i < m->files && n < count; i++, n++) {
        memset(&info[n], 0, sizeof(info[n]));
        info[n].content_id.c[0] = (u8)(m->type);
        info[n].content_id.c[1] = (u8)i;
        info[n].content_id.c[2] = (u8)(i < m->files_present);   // the storage reads this back
    }
    *written = n;
    return 0;
}

Result ncmContentStorageHas(NcmContentStorage *cs, bool *out, const NcmContentId *id)
{
    assert(cs->open);
    *out = id->c[2] != 0;
    return 0;
}

// ------------------------------------------------------------ helpers

#define HOS(a, b, c) MAKEHOSVERSION(a, b, c)
#define SYSVER(a, b, c) (((u32)(a) << 26) | ((u32)(b) << 20) | ((u32)(c) << 16))
static const u32 CURRENT = HOS(22, 1, 0);

static GameCheck R;

static const GameFacts *facts(u64 id)
{
    for (u32 i = 0; i < R.count; i++)
        if (R.games[i].app_id == id) return &R.games[i];
    assert(!"game not scanned");
    return NULL;
}

static GameIssue issue_of(u64 id)
{
    return gamecheck_classify(facts(id), CURRENT);
}

static void scan(void)
{
    gamecheck_scan(&R);
    assert(C.ns_open == 0 && C.ncm_open == 0 && C.db_open == 0 && C.cs_open == 0);
}

// ------------------------------------------------------------ tests

static void test_sysver(void)
{
    assert(gamecheck_sysver_to_hos(0) == 0);
    assert(gamecheck_sysver_to_hos(SYSVER(1, 0, 0)) == HOS(1, 0, 0));
    assert(gamecheck_sysver_to_hos(SYSVER(20, 0, 0)) == HOS(20, 0, 0));
    assert(gamecheck_sysver_to_hos(SYSVER(21, 2, 1) | 0x0) == HOS(21, 2, 1));
    assert(gamecheck_sysver_to_hos(SYSVER(19, 0, 1) | 0x1A) == HOS(19, 0, 1));   // the relstep ignored
    assert(gamecheck_sysver_to_hos(0x50000000u) == HOS(20, 0, 0));
}

static void test_every_problem(void)
{
    reset();
    FakeGame *g;
    g = add_game(0x0100000000010000ULL, "Fine");   // game + update on the SD card, add-on
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0)->required_sysver = SYSVER(12, 0, 0);
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x30000)->required_sysver = SYSVER(20, 0, 0);
    add_meta(g, NcmContentMetaType_AddOnContent, NcmStorageId_SdCard, 0)->registered = false;   // add-ons not checked

    g = add_game(0x0100000000020000ULL, "Archived");
    g = add_game(0x0100000000030000ULL, "Update only");
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x10000);
    g = add_game(0x0100000000040000ULL, "Missing file");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_BuiltInUser, 0);
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x10000)->files_present = 1;
    g = add_game(0x0100000000050000ULL, "Unknown to ncm");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0)->registered = false;
    g = add_game(0x0100000000060000ULL, "Too new");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0)->required_sysver = SYSVER(20, 0, 0);
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x20000)->required_sysver = SYSVER(23, 0, 0);
    g = add_game(0x0100000000070000ULL, "Refused");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0);
    g->launch_rc = 0xABCD;
    g = add_game(0x0100000000080000ULL, NULL);   // the icon that keeps loading
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0);
    g = add_game(0x0100000000090000ULL, "Card");   // a card out of its slot
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_GameCard, 0)->registered = false;
    g = add_game(0x01000000000A0000ULL, "Card with an update");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_GameCard, 0)->registered = false;
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x10000);
    g = add_game(0x01000000000B0000ULL, "Same version");   // exactly the console's: fine
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0)->required_sysver = SYSVER(22, 1, 0);

    scan();
    assert(R_SUCCEEDED(R.rc) && R.count == (u32)C.count && !R.truncated);

    const GameFacts *f = facts(0x0100000000010000ULL);
    assert(f->control_ok && strcmp(f->name, "Fine") == 0);
    assert(f->meta_count == 3 && f->has_base && f->has_patch && f->patch_version == 0x30000);
    assert(!f->card_only && !f->meta_missing && !f->files_missing && !f->storage_unavailable);
    assert(f->required_hos == HOS(20, 0, 0));   // the highest of the game and its update
    assert(issue_of(0x0100000000010000ULL) == GC_OK);

    assert(facts(0x0100000000020000ULL)->meta_count == 0);
    assert(issue_of(0x0100000000020000ULL) == GC_ARCHIVED);
    assert(issue_of(0x0100000000030000ULL) == GC_NO_BASE);
    assert(facts(0x0100000000040000ULL)->files_missing && issue_of(0x0100000000040000ULL) == GC_FILES_MISSING);
    assert(facts(0x0100000000050000ULL)->meta_missing && issue_of(0x0100000000050000ULL) == GC_FILES_MISSING);
    assert(facts(0x0100000000060000ULL)->required_hos == HOS(23, 0, 0));
    assert(issue_of(0x0100000000060000ULL) == GC_FIRMWARE_TOO_OLD);
    assert(facts(0x0100000000070000ULL)->launch_rc == 0xABCD && issue_of(0x0100000000070000ULL) == GC_LAUNCH_REFUSED);
    f = facts(0x0100000000080000ULL);
    assert(!f->control_ok && f->name[0] == '\0' && issue_of(0x0100000000080000ULL) == GC_NO_CONTROL);
    assert(facts(0x0100000000090000ULL)->card_only && issue_of(0x0100000000090000ULL) == GC_OK);
    f = facts(0x01000000000A0000ULL);
    assert(!f->card_only && f->has_base && issue_of(0x01000000000A0000ULL) == GC_OK);   // the card is not read
    assert(issue_of(0x01000000000B0000ULL) == GC_OK);
}

static void test_sd_card_unreadable(void)
{
    reset();
    FakeGame *g = add_game(0x0100000000010000ULL, "On the SD card");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0);
    g = add_game(0x0100000000020000ULL, "In the console");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_BuiltInUser, 0);
    C.sd_unopenable = true;
    scan();
    assert(facts(0x0100000000010000ULL)->storage_unavailable);
    assert(issue_of(0x0100000000010000ULL) == GC_STORAGE_UNAVAILABLE);
    assert(!gamecheck_removable(GC_STORAGE_UNAVAILABLE));
    assert(issue_of(0x0100000000020000ULL) == GC_OK);
}

static void test_without_ncm(void)
{
    // Only what ns says: nothing reported missing for want of ncm.
    reset();
    FakeGame *g = add_game(0x0100000000010000ULL, "Fine");
    add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0)->files_present = 0;
    g = add_game(0x0100000000020000ULL, "Update only");
    add_meta(g, NcmContentMetaType_Patch, NcmStorageId_SdCard, 0x10000);
    C.ncm_init_rc = 0x9999;
    scan();
    const GameFacts *f = facts(0x0100000000010000ULL);
    assert(!f->storage_unavailable && !f->files_missing && !f->meta_missing);
    assert(issue_of(0x0100000000010000ULL) == GC_OK);
    assert(issue_of(0x0100000000020000ULL) == GC_NO_BASE);
}

static void test_list_failures(void)
{
    reset();
    C.ns_init_rc = 0x1111;
    gamecheck_scan(&R);
    assert(R.rc == 0x1111 && R.count == 0 && C.ns_open == 0);

    reset();
    add_game(0x0100000000010000ULL, "A");
    C.list_rc = 0x2222;
    scan();
    assert(R.rc == 0x2222 && R.count == 0);
}

static void test_paging(void)
{
    // More games than one chunk of records, more than the list holds, and
    // more contents than one chunk for a game.
    reset();
    for (int i = 0; i < GAMECHECK_MAX + 10; i++) {
        FakeGame *g = add_game(0x0100000000000000ULL + ((u64)(i + 1) << 16), "Game");
        add_meta(g, NcmContentMetaType_Application, NcmStorageId_SdCard, 0);
    }
    FakeGame *many = &C.games[0];
    for (int i = 0; i < 5; i++) add_meta(many, NcmContentMetaType_AddOnContent, NcmStorageId_SdCard, 0);
    scan();
    assert(R_SUCCEEDED(R.rc) && R.count == GAMECHECK_MAX && R.truncated);
    assert(R.games[GAMECHECK_MAX - 1].app_id == C.games[GAMECHECK_MAX - 1].id);
    assert(facts(many->id)->meta_count == 6);
    for (u32 i = 0; i < R.count; i++) assert(gamecheck_classify(&R.games[i], CURRENT) == GC_OK);

    reset();
    for (int i = 0; i < 64; i++) add_game(0x0100000000000000ULL + ((u64)(i + 1) << 16), "Game");   // exactly one chunk
    scan();
    assert(R.count == 64 && !R.truncated);
}

static bool s_confirm;
static bool confirm(void) { return s_confirm; }

static void test_remove(void)
{
    reset();
    assert(gamecheck_remove(0x0100000000010000ULL) == 0 && C.deleted == 0x0100000000010000ULL && C.ns_open == 0);

    reset();
    C.delete_rc = 0x7777;
    assert(gamecheck_remove(0x0100000000010000ULL) == 0x7777 && C.ns_open == 0);

    // Read-only, the PIN refused: no session opened.
    reset();
    core_set_read_only(true);
    assert(gamecheck_remove(0x0100000000010000ULL) == NXM_RC_READ_ONLY && C.ns_inits == 0 && C.deleted == 0);
    core_set_read_only(false);
    core_set_change_check(confirm);
    s_confirm = false;
    assert(gamecheck_remove(0x0100000000010000ULL) == NXM_RC_NOT_CONFIRMED && C.ns_inits == 0 && C.deleted == 0);
    s_confirm = true;
    assert(gamecheck_remove(0x0100000000010000ULL) == 0 && C.deleted == 0x0100000000010000ULL);
    core_set_change_check(NULL);

    reset();
    C.ns_init_rc = 0x1111;
    assert(gamecheck_remove(0x0100000000010000ULL) == 0x1111 && C.deleted == 0);
}

static void test_rules(void)
{
    // The order: the first that applies explains the others.
    GameFacts g;
    memset(&g, 0, sizeof(g));
    assert(gamecheck_classify(&g, CURRENT) == GC_ARCHIVED);
    g.meta_count = 1;
    g.files_missing = true;
    g.control_ok = false;
    g.required_hos = HOS(30, 0, 0);
    g.launch_rc = 1;
    assert(gamecheck_classify(&g, CURRENT) == GC_NO_BASE);
    g.has_base = true;
    assert(gamecheck_classify(&g, CURRENT) == GC_FILES_MISSING);
    g.files_missing = false;
    assert(gamecheck_classify(&g, CURRENT) == GC_FIRMWARE_TOO_OLD);
    assert(gamecheck_classify(&g, 0) == GC_LAUNCH_REFUSED);   // the console's version unknown: not judged
    g.required_hos = 0;
    assert(gamecheck_classify(&g, CURRENT) == GC_LAUNCH_REFUSED);
    g.launch_rc = 0;
    assert(gamecheck_classify(&g, CURRENT) == GC_NO_CONTROL);
    g.control_ok = true;
    assert(gamecheck_classify(&g, CURRENT) == GC_OK);
    g.storage_unavailable = true;
    assert(gamecheck_classify(&g, CURRENT) == GC_STORAGE_UNAVAILABLE);

    assert(!gamecheck_removable(GC_OK));
    assert(!gamecheck_removable(GC_STORAGE_UNAVAILABLE));
    assert(!gamecheck_removable(GC_ISSUE_COUNT));
    assert(gamecheck_removable(GC_ARCHIVED) && gamecheck_removable(GC_NO_BASE) && gamecheck_removable(GC_FILES_MISSING) &&
           gamecheck_removable(GC_FIRMWARE_TOO_OLD) && gamecheck_removable(GC_LAUNCH_REFUSED) &&
           gamecheck_removable(GC_NO_CONTROL));
}

int main(void)
{
    test_sysver();
    test_rules();
    test_every_problem();
    test_sd_card_unreadable();
    test_without_ncm();
    test_list_failures();
    test_paging();
    test_remove();
    puts("gamecheck assertions passed");
    return 0;
}
