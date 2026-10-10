// agent_update — puts the agent sysmodule PlayGuard carries in place of the
// one on the SD card while it runs, and puts the previous one back when the
// new one does not answer (the steps: util/modules.hpp, next_step). Each step
// runs on the UI thread; the wait for the new agent's Hello (10 s at most)
// does not block the screen. Also the offer at start-up, when the agent
// installed is not the one PlayGuard carries.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

#include "util/modules.hpp"

namespace brls
{
class Activity;
}

namespace agent_update
{

struct Outcome
{
    modules::Step end = modules::Step::Failed;   // Done, RolledBack or Failed
    bool          update = false;                // an agent was there before
    std::string   why;                           // when it went wrong
};

// Installs or updates the agent from `bundle_dir` (Tools › Optional modules'
// copy), records it in the change history, then hands the link over again;
// `done` gets the outcome. The PIN is the caller's.
void run(const std::string& bundle_dir, std::function<void(const Outcome&)> done);
bool running();

// Once the main screen is up, with nothing in front of it: the agent
// installed is another build than the one PlayGuard carries (or speaks
// another protocol), and the parent did not answer "Later" to this one.
void at_start(brls::Activity* main, const std::string& bundle_dir);

// The toast or dialog for an outcome of run().
void tell(const Outcome& o);

}   // namespace agent_update
