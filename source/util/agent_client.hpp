// agent_client — PlayGuard's side of the pg:agent service
// (source/sync/agent_ipc.h): when the agent sysmodule runs, PlayGuard does
// not talk to the broker itself; it pushes what it reads of the console and
// carries out the orders the agent hands it. On the console through
// serviceDispatch (agent_client_nx.cpp); on the desktop an agent simulated in
// the process (source/sim/sim_agent.cpp, PLAYGUARD_SIM_AGENT=running).
// UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "sync/agent_ipc.h"
#include "util/pctl_ops_c.hpp"

namespace agent_client
{

// The agent has registered pg:agent (it runs). Never blocks.
bool available();
// Connects and says Hello. True when the reply came (*reply.accepted says
// whether the session is PlayGuard's); false when the agent cannot be reached.
bool open(AgentHelloReply* reply, Result* rc = nullptr);
void close();
bool connected();

Result set_foreground(bool on);
Result push(AgentCmd cmd, const std::string& doc);                 // PushState, PushActivity, PushNames, PushWeek
Result push_final(const std::string& date, const std::string& doc); // activity/<date> ("" clears it)
Result pop_order(AgentOrder* out);
Result order_result(const AgentResult& r);
Result sync_now();
Result reload();
Result prepare_shutdown();
Result status(AgentStatus* out);
Result records(AgentRecords* out);
Result log(std::string* out);

}   // namespace agent_client
