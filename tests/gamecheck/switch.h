// Minimal fake <switch.h> for the gamecheck host tests: the ns and ncm calls
// core/gamecheck.c makes, played by tests/gamecheck/test.c.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define MAKEHOSVERSION(_major, _minor, _micro) (((u32)(_major) << 16) | ((u32)(_minor) << 8) | (u32)(_micro))

typedef enum {
    NcmStorageId_None = 0, NcmStorageId_Host = 1, NcmStorageId_GameCard = 2, NcmStorageId_BuiltInSystem = 3,
    NcmStorageId_BuiltInUser = 4, NcmStorageId_SdCard = 5, NcmStorageId_Any = 6,
} NcmStorageId;
typedef enum {
    NcmContentMetaType_Application = 0x80, NcmContentMetaType_Patch = 0x81, NcmContentMetaType_AddOnContent = 0x82,
} NcmContentMetaType;
typedef enum { NcmContentInstallType_Full = 0 } NcmContentInstallType;

typedef struct { u8 c[0x10]; } NcmContentId;
typedef struct {
    u64 id;
    u32 version;
    u8 type;
    u8 install_type;
    u8 padding[2];
} NcmContentMetaKey;
typedef struct {
    NcmContentId content_id;
    u32 size_low;
    u8 size_high, attr, content_type, id_offset;
} NcmContentInfo;
typedef struct { NcmStorageId storage; bool open; } NcmContentMetaDatabase;
typedef struct { NcmStorageId storage; bool open; } NcmContentStorage;

Result ncmInitialize(void);
void ncmExit(void);
Result ncmOpenContentMetaDatabase(NcmContentMetaDatabase *out, NcmStorageId storage_id);
Result ncmOpenContentStorage(NcmContentStorage *out, NcmStorageId storage_id);
void ncmContentMetaDatabaseClose(NcmContentMetaDatabase *db);
void ncmContentStorageClose(NcmContentStorage *cs);
Result ncmContentMetaDatabaseHas(NcmContentMetaDatabase *db, bool *out, const NcmContentMetaKey *key);
Result ncmContentMetaDatabaseGetRequiredSystemVersion(NcmContentMetaDatabase *db, u32 *out_version, const NcmContentMetaKey *key);
Result ncmContentMetaDatabaseListContentInfo(NcmContentMetaDatabase *db, s32 *out_entries_written, NcmContentInfo *out_info,
                                             s32 count, const NcmContentMetaKey *key, s32 start_index);
Result ncmContentStorageHas(NcmContentStorage *cs, bool *out, const NcmContentId *content_id);

typedef struct { char name[0x200]; char author[0x100]; } NacpLanguageEntry;
typedef struct { NacpLanguageEntry lang[16]; u8 rest[0x1000]; } NacpStruct;
typedef struct { NacpStruct nacp; u8 icon[0x20000]; } NsApplicationControlData;
typedef enum { NsApplicationControlSource_CacheOnly = 0, NsApplicationControlSource_Storage = 1 } NsApplicationControlSource;
typedef struct {
    u64 application_id;
    u8 last_event, attributes, reserved[6];
    u64 last_updated;
} NsApplicationRecord;
typedef struct {
    u8 meta_type, storageID, rights_check, reserved;
    u32 version;
    u64 application_id;
} NsApplicationContentMetaStatus;

Result nsInitialize(void);
void nsExit(void);
Result nsListApplicationRecord(NsApplicationRecord *records, s32 count, s32 entry_offset, s32 *out_entrycount);
Result nsGetApplicationControlData(NsApplicationControlSource source, u64 application_id, NsApplicationControlData *buffer,
                                   size_t size, u64 *actual_size);
Result nacpGetLanguageEntry(NacpStruct *nacp, NacpLanguageEntry **langentry);
Result nsListApplicationContentMetaStatus(u64 application_id, s32 index, NsApplicationContentMetaStatus *list, s32 count,
                                          s32 *out_entrycount);
Result nsCheckApplicationLaunchVersion(u64 application_id);
Result nsDeleteApplicationCompletely(u64 application_id);
