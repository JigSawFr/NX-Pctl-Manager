// sync_orders — an order from the remote link (Home Assistant, MQTT) carried
// out by PlayGuard while it runs, on the UI thread: with the link's policy
// "ask", the usual confirmation first (and the PIN when Security asks for it
// before a change); with "auto", at once, the PIN check skipped (the broker
// authenticated the sender). The console call itself is the shared
// source/sync/sync_exec.c (the same gate, unlock and relock as a press on the
// console); this side keeps config.json's records, the change history (source
// "remote") and the toasts.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "sync/sync_apply.h"
#include "sync/sync_conf.h"
#include "sync/sync_exec.h"
#include "util/sync_files.hpp"

namespace sync_orders
{

struct Order
{
    uint32_t    id = 0;
    SyncIntent  intent{};
    std::string entity, payload;
    bool        retained = false;
};

// What the order asks, as a sentence ("Set Monday's limit to 2 h").
std::string describe(const SyncIntent& in);

// Runs `o` (a console intent) with `policy` and calls `done` once with the
// outcome: at once, or after the confirmation's answer.
void run(const Order& o, SyncPolicy policy, bool remote_timer_writes, std::function<void(const SyncOutcome&)> done);

// The toast or dialog that says how an order ended.
void tell(const Order& o, const SyncOutcome& out);

// A change the agent made while PlayGuard was closed (sync/agent_events.log),
// recorded in the change history at `when` ("2026-10-08 18:30").
void import(const sync_files::AgentEvent& ev, const std::string& when);

}   // namespace sync_orders
