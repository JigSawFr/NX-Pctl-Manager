// github_login_flow — Developer tools › GitHub account: signs in with the
// device flow (util/github_auth.hpp), a code and a QR code to scan, or signs
// out (forgets the token).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>

namespace github_login_flow
{

// Signed in: offers to sign out; otherwise signs in. `done` runs after a change.
void open(std::function<void()> done = nullptr);

// Signs in; `done` runs once the token is saved.
void sign_in(std::function<void()> done = nullptr);

}   // namespace github_login_flow
