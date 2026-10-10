// PlayGuard — the installed games the HOME menu cannot start or draw: an
// icon that keeps loading, a "!" or a dotted frame instead of the game. For
// each game the console lists (ns), what is installed of it (ns, ncm): the
// game itself and its update, where, whether their files are there and the
// system version they ask for. gamecheck_classify() turns that into one
// problem and the UI into a cause and a fix; gamecheck_remove() deletes a
// broken game the way System Settings › Data Management does (save data
// kept).
//
// The scan only reads, one ns and ncm session for the whole list, closed
// before it returns; it reads every game, so the UI runs it off the main
// thread. Removing a game is a change: it goes through core_change_allowed()
// (read-only mode, the PIN asked before a change).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAMECHECK_MAX      512
#define GAMECHECK_NAME_LEN 128

// What was found of one game. Filled by gamecheck_scan(); the host tests and
// the simulator fill it by hand.
typedef struct {
    u64  app_id;
    char name[GAMECHECK_NAME_LEN];   // UTF-8, console language; empty when unreadable
    bool control_ok;                 // name and icon read (what the HOME menu draws)

    u32  meta_count;                 // contents the console lists for it (game, update, add-ons)
    bool has_base;                   // the game itself
    bool has_patch;                  // an update
    u32  base_version, patch_version;
    bool card_only;                  // every content on a game card (one not inserted is normal)
    bool storage_unavailable;        // a content on a storage that could not be opened (SD card)
    bool meta_missing;               // a listed content the storage does not know
    bool files_missing;              // a file of the game or its update missing from its storage
    u32  required_hos;               // MAKEHOSVERSION the game or its update needs, 0 when unknown
    Result launch_rc;                // nsCheckApplicationLaunchVersion: why the console would refuse it
} GameFacts;

// The one problem shown for a game, the first that applies in this order: an
// earlier one explains the later ones (no files: no name either).
typedef enum {
    GC_OK = 0,
    GC_ARCHIVED,             // listed with nothing installed: the dotted frame
    GC_STORAGE_UNAVAILABLE,  // on the SD card, which cannot be read
    GC_NO_BASE,              // an update or add-on without the game: "!"
    GC_FILES_MISSING,        // installed, files missing or unknown to the storage
    GC_FIRMWARE_TOO_OLD,     // needs a newer system version than this console's
    GC_LAUNCH_REFUSED,       // the console refuses to start it (an update is required)
    GC_NO_CONTROL,           // everything there but the name and icon: an icon that keeps loading
    GC_ISSUE_COUNT
} GameIssue;

// `current_hos`: the console's MAKEHOSVERSION.
GameIssue gamecheck_classify(const GameFacts *g, u32 current_hos);

// Whether PlayGuard offers to remove the game for `issue`: never for a game
// that is fine or whose SD card cannot be read (nothing to clean there, and
// the files would stay behind on the card).
bool gamecheck_removable(GameIssue issue);

// A content meta's RequiredSystemVersion (major << 26 | minor << 20 |
// micro << 16 | relstep) as MAKEHOSVERSION; 0 stays 0.
u32 gamecheck_sysver_to_hos(u32 required_system_version);

typedef struct {
    Result rc;          // listing the games (ns); nothing else is filled when it fails
    u32    count;       // every game the console lists, up to GAMECHECK_MAX
    bool   truncated;   // more than GAMECHECK_MAX games
    GameFacts games[GAMECHECK_MAX];
} GameCheck;

void gamecheck_scan(GameCheck *out);

// Deletes the game, its update and add-ons, and its entry on the HOME menu
// (nsDeleteApplicationCompletely). Save data stays. NXM_RC_READ_ONLY /
// NXM_RC_NOT_CONFIRMED before any session is opened.
Result gamecheck_remove(u64 app_id);

#ifdef __cplusplus
}
#endif
