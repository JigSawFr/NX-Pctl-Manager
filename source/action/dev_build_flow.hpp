// dev_build_flow — Developer tools › Install another build: lists the latest
// release, the last commits of main and the open pull requests
// (util/dev_builds.hpp), downloads the one picked, checks it, puts it in
// place of the running .nro and restarts PlayGuard on it through hbloader.
// The list is kept (memory and cache/dev_builds.json) and shown at once for
// ten minutes; its last line fetches it again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace dev_build_flow
{

// `force`: fetch the list even when the one kept is fresh.
void open(bool force = false);

}   // namespace dev_build_flow
