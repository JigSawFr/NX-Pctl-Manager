// dev_build_flow — Developer tools › Install another build: lists the latest
// release, the last commits of main and the open pull requests
// (util/dev_builds.hpp), downloads the one picked, checks it, puts it in
// place of the running .nro and restarts PlayGuard on it through hbloader.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace dev_build_flow
{

void open();

}   // namespace dev_build_flow
