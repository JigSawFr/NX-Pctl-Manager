// http — a small HTTPS GET on libcurl. On the Switch, devkitPro's switch-curl
// does TLS through the console's ssl service, which verifies servers against
// the system's built-in CA store; on desktop it is the system libcurl.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <string>

namespace http
{

// Follows redirects. False (and *error set, in English) on a network error,
// an HTTP status >= 400, a timeout or a body larger than `max_bytes`.
// Blocking: call it from brls::async.
bool get(const std::string& url, std::string* body, std::string* error,
         size_t max_bytes = 64 * 1024, long timeout_s = 10);

// Releases libcurl's global state (and the Switch ssl service). At exit;
// does nothing while a request is still running.
void cleanup();

}   // namespace http
